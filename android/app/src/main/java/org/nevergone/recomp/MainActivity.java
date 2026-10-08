package org.nevergone.recomp;

import android.app.Activity;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("nevergone_recomp");
    }

    private static native String nativeBootstrapInfo(String filesDir, String deviceId);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String deviceId = Settings.Secure.getString(
                getContentResolver(), Settings.Secure.ANDROID_ID);
        if (deviceId == null || deviceId.isEmpty()) {
            deviceId = "recomp-device-unknown";
        }

        TextView status = new TextView(this);
        status.setGravity(Gravity.CENTER);
        status.setTextSize(18.0f);
        status.setPadding(32, 32, 32, 32);
        status.setText(
                "Never Gone Recomp\n\n"
                        + nativeBootstrapInfo(getFilesDir().getAbsolutePath(), deviceId));
        setContentView(status);
    }
}
