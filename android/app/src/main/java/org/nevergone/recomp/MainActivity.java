package org.nevergone.recomp;

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("nevergone_recomp");
    }

    private static native String nativeBootstrapInfo();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        TextView status = new TextView(this);
        status.setGravity(Gravity.CENTER);
        status.setTextSize(18.0f);
        status.setPadding(32, 32, 32, 32);
        status.setText("Never Gone Recomp\n\n" + nativeBootstrapInfo());
        setContentView(status);
    }
}
