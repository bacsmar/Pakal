package org.pakalengine;

import android.app.NativeActivity;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
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
