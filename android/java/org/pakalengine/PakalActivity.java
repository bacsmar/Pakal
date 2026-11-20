package org.pakalengine;

import android.app.NativeActivity;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.net.Uri;
import android.provider.MediaStore;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

/**
 * The game's activity: the engine's NativeActivity, full screen. The system bars (status and navigation) are hidden and
 * come back for a moment with a swipe from the edge; the picture also covers the camera cutout. This has to run on the
 * activity's own thread, which the native code does not have, hence this small class.
 * A game that plays online (its manifest asks for CHANGE_WIFI_MULTICAST_STATE) also keeps the Wi-Fi taking broadcasts
 * while it runs: many phones drop them to save power, and the games on the network are found by broadcast.
 */
public class PakalActivity extends NativeActivity {
    private WifiManager.MulticastLock broadcasts;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams attributes = getWindow().getAttributes();
            attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attributes);
        }
        immersive();
        keepLink(getIntent());
        try {
            WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
            if (wifi != null) {
                broadcasts = wifi.createMulticastLock("pakal");
                broadcasts.setReferenceCounted(false);
                broadcasts.acquire();
            }
        } catch (SecurityException notAsked) {
            broadcasts = null; // a game that does not play online
        }
    }

    /**
     * A POST to a server (http or https), waiting for its answer: the HTTP status, or -1 when it could not be sent. 'headers'
     * are names and values in turn. Called by the native code from a thread of its own (never the activity's: it blocks).
     * The app's manifest must ask for INTERNET.
     */
    public static int postBytes(String url, String[] headers, byte[] body, int timeoutMs) {
        java.net.HttpURLConnection connection = null;
        try {
            connection = (java.net.HttpURLConnection) new java.net.URL(url).openConnection();
            connection.setRequestMethod("POST");
            connection.setConnectTimeout(timeoutMs);
            connection.setReadTimeout(timeoutMs);
            connection.setDoOutput(true);
            connection.setUseCaches(false);
            for (int i = 0; i + 1 < headers.length; i += 2) connection.setRequestProperty(headers[i], headers[i + 1]);
            connection.setFixedLengthStreamingMode(body.length);
            OutputStream out = connection.getOutputStream();
            out.write(body);
            out.close();
            int status = connection.getResponseCode();
            InputStream answer = status < 400 ? connection.getInputStream() : connection.getErrorStream();
            if (answer != null) {
                byte[] drain = new byte[1024];
                while (answer.read(drain) > 0) { }
                answer.close();
            }
            return status;
        } catch (Exception failed) {
            return -1;
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    /** The link that opened the game (a VIEW intent: android.json "links"), or the one that reached it while it ran; null: none */
    private String link;

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        keepLink(intent);
    }

    private synchronized void keepLink(Intent intent) {
        if (intent != null && Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() != null) {
            link = intent.getData().toString();
        }
    }

    /** The link that opened the game, once (the next call: null until another comes). Called by the native code, from any thread. */
    public synchronized String takeLink() {
        String taken = link;
        link = null;
        return taken;
    }

    /** The share sheet with a text (a message with a link: chat, e-mail). Called by the native code, from any thread. */
    public void shareText(final String text, final String title) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent send = new Intent(Intent.ACTION_SEND);
                    send.setType("text/plain");
                    send.putExtra(Intent.EXTRA_TEXT, text);
                    startActivity(Intent.createChooser(send, title));
                } catch (Exception failed) {
                    android.util.Log.w("Pakal", "share text: " + failed);
                }
            }
        });
    }

    /** Opens a web address (http/https) in the browser. Called by the native code, from any thread. */
    public void openUrl(final String url) {
        if (!url.startsWith("http://") && !url.startsWith("https://")) return;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent view = new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(url));
                    view.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(view);
                } catch (Exception failed) {
                    android.util.Log.w("Pakal", "cannot open " + url + ": " + failed);
                }
            }
        });
    }

    /**
     * Any request (GET, POST...) with these headers (name, value, name, value...) and body (none: empty), its answer's body written
     * to 'outPath' (a file of the app's): the HTTP status, or -1. Called by the native code from a thread of its own.
     */
    public static int requestBytes(String method, String url, String[] headers, byte[] body, int timeoutMs, String outPath) {
        java.net.HttpURLConnection connection = null;
        try {
            connection = (java.net.HttpURLConnection) new java.net.URL(url).openConnection();
            connection.setRequestMethod(method);
            connection.setConnectTimeout(timeoutMs);
            connection.setReadTimeout(timeoutMs);
            connection.setUseCaches(false);
            for (int i = 0; i + 1 < headers.length; i += 2) connection.setRequestProperty(headers[i], headers[i + 1]);
            if (body != null && body.length > 0) {
                connection.setDoOutput(true);
                connection.setFixedLengthStreamingMode(body.length);
                OutputStream out = connection.getOutputStream();
                out.write(body);
                out.close();
            }
            int status = connection.getResponseCode();
            InputStream answer = status < 400 ? connection.getInputStream() : connection.getErrorStream();
            java.io.FileOutputStream file = new java.io.FileOutputStream(outPath);
            if (answer != null) {
                byte[] chunk = new byte[16384];
                for (int n; (n = answer.read(chunk)) > 0; ) file.write(chunk, 0, n);
                answer.close();
            }
            file.close();
            return status;
        } catch (Exception failed) {
            return -1;
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    /**
     * Hands a file the game wrote (a profiling session) to the person: the share sheet (e-mail, chat, a drive).
     * From Android 10 a copy goes to Downloads first (MediaStore), which the share passes on as is; before that, its
     * text. Called by the native code, from any thread.
     */
    public void shareFile(final String path, final String title) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    File file = new File(path);
                    Intent send = new Intent(Intent.ACTION_SEND);
                    send.setType("text/plain");
                    send.putExtra(Intent.EXTRA_SUBJECT, title + ": " + file.getName());
                    if (Build.VERSION.SDK_INT >= 29) {
                        ContentValues values = new ContentValues();
                        values.put(MediaStore.MediaColumns.DISPLAY_NAME, file.getName());
                        values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
                        Uri copy = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
                        if (copy == null) {
                            return;
                        }
                        try (InputStream in = new FileInputStream(file); OutputStream out = getContentResolver().openOutputStream(copy)) {
                            byte[] buffer = new byte[16384];
                            for (int read; (read = in.read(buffer)) > 0; ) {
                                out.write(buffer, 0, read);
                            }
                        }
                        send.putExtra(Intent.EXTRA_STREAM, copy);
                        send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    } else {
                        byte[] bytes = new byte[(int) file.length()];
                        try (InputStream in = new FileInputStream(file)) {
                            int at = 0;
                            for (int read; at < bytes.length && (read = in.read(bytes, at, bytes.length - at)) > 0; ) {
                                at += read;
                            }
                        }
                        send.putExtra(Intent.EXTRA_TEXT, new String(bytes, StandardCharsets.UTF_8));
                    }
                    startActivity(Intent.createChooser(send, title));
                } catch (Exception failed) {
                    android.util.Log.w("Pakal", "share " + path + ": " + failed);
                }
            }
        });
    }

    /**
     * The certificates this package is signed with, as SHA-256 in lowercase hex, separated by commas (with a rotated key,
     * every one of its history); empty when the system does not tell. Called by the native code at start.
     */
    @SuppressWarnings("deprecation")
    public String signerDigests() {
        try {
            PackageManager manager = getPackageManager();
            Signature[] signatures;
            if (Build.VERSION.SDK_INT >= 28) {
                PackageInfo info = manager.getPackageInfo(getPackageName(), PackageManager.GET_SIGNING_CERTIFICATES);
                signatures = info.signingInfo == null ? null : info.signingInfo.hasMultipleSigners()
                        ? info.signingInfo.getApkContentsSigners() : info.signingInfo.getSigningCertificateHistory();
            } else {
                signatures = manager.getPackageInfo(getPackageName(), PackageManager.GET_SIGNATURES).signatures;
            }
            StringBuilder digests = new StringBuilder();
            for (Signature signature : signatures == null ? new Signature[0] : signatures) {
                byte[] digest = MessageDigest.getInstance("SHA-256").digest(signature.toByteArray());
                if (digests.length() > 0) {
                    digests.append(',');
                }
                for (byte b : digest) {
                    digests.append(String.format("%02x", b & 0xff));
                }
            }
            return digests.toString();
        } catch (Exception failed) {
            android.util.Log.w("Pakal", "signers: " + failed);
            return "";
        }
    }

    @Override
    protected void onDestroy() {
        if (broadcasts != null) {
            broadcasts.release();
        }
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            immersive(); // the bars come back with dialogs and the app switcher
        }
    }

    private void immersive() {
        if (Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        }
    }
}
