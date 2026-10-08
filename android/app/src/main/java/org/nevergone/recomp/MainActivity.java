package org.nevergone.recomp;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.TextView;

import java.util.UUID;

public final class MainActivity extends Activity {
    private static final String PREFS = "nevergone_recomp_runtime";
    private static final String DEVICE_ID = "device_id";

    static {
        System.loadLibrary("nevergone_recomp");
    }

    private static native void nativeConfigureRuntime(String filesDir, String deviceId);
    private static native String nativeBootstrapInfo();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        nativeConfigureRuntime(getFilesDir().getAbsolutePath(), getOrCreateDeviceId());

        TextView status = new TextView(this);
        status.setGravity(Gravity.CENTER);
        status.setTextSize(18.0f);
        status.setPadding(32, 32, 32, 32);
        status.setText("Never Gone Recomp\n\n" + nativeBootstrapInfo());
        setContentView(status);
    }

    private String getOrCreateDeviceId() {
        SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        String value = prefs.getString(DEVICE_ID, null);
        if (value != null && !value.isEmpty()) {
            return value;
        }

        value = UUID.randomUUID().toString();
        prefs.edit().putString(DEVICE_ID, value).apply();
        return value;
    }
}
