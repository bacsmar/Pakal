package org.pakalengine;

import android.app.NativeActivity;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

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
