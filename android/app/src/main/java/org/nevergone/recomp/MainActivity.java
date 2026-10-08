package org.nevergone.recomp;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.util.Locale;
import java.util.UUID;

public final class MainActivity extends Activity {
    private static final String PREFS = "nevergone_recomp_runtime";
    private static final String DEVICE_ID = "device_id";
    private static final int REQUEST_ORIGINAL_APK = 1001;

    private TextView status;
    private Button importButton;
    private GameSurfaceView gameSurface;

    static {
        System.loadLibrary("nevergone_recomp");
    }

    private static native void nativeConfigureRuntime(String filesDir, String deviceId, String appVersion);
    private static native String nativeBootstrapInfo();
    private static native String nativeClientUiState();
    private static native void nativeOnAppPause();
    private static native void nativeOnAppResume();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        nativeConfigureRuntime(
                getFilesDir().getAbsolutePath(),
                getOrCreateDeviceId(),
                getAppVersion());

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);

        gameSurface = new GameSurfaceView(this);
        root.addView(gameSurface, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                0,
                3.0f));

        LinearLayout content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(32, 16, 32, 32);

        importButton = new Button(this);
        importButton.setText("Import original Never Gone APK");
        importButton.setOnClickListener(view -> chooseOriginalApk());
        content.addView(importButton, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        Button refreshButton = new Button(this);
        refreshButton.setText("Refresh runtime diagnostics");
        refreshButton.setOnClickListener(view -> status.setText(buildStatusText(null)));
        content.addView(refreshButton, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        status = new TextView(this);
        status.setGravity(Gravity.START);
        status.setTextSize(14.0f);
        status.setPadding(0, 16, 0, 24);
        status.setText(buildStatusText(null));
        content.addView(status, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        ScrollView scroll = new ScrollView(this);
        scroll.addView(content);
        root.addView(scroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                0,
                2.0f));

        setContentView(root);
    }

    @Override
    protected void onPause() {
        nativeOnAppPause();
        if (gameSurface != null) {
            gameSurface.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (gameSurface != null) {
            gameSurface.onResume();
        }
        nativeOnAppResume();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_ORIGINAL_APK || resultCode != RESULT_OK || data == null) {
            return;
        }
        Uri apkUri = data.getData();
        if (apkUri == null) {
            status.setText(buildStatusText("Import failed: no file selected."));
            return;
        }
        importOriginalApk(apkUri);
    }

    private void chooseOriginalApk() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/vnd.android.package-archive");
        startActivityForResult(intent, REQUEST_ORIGINAL_APK);
    }

    private void importOriginalApk(Uri apkUri) {
        importButton.setEnabled(false);
        status.setText(buildStatusText("Importing and decoding original APK assets..."));

        new Thread(() -> {
            try {
                OriginalApkImporter.Result result = OriginalApkImporter.importAssets(
                        getContentResolver(), apkUri, getFilesDir());
                String summary = String.format(
                        Locale.ROOT,
                        "Imported %d files (%.2f MiB); decoded %d encoded assets. " +
                                "Startup diagnostics rerun below.",
                        result.files,
                        result.bytes / (1024.0 * 1024.0),
                        result.decodedFiles);
                runOnUiThread(() -> {
                    importButton.setEnabled(true);
                    status.setText(buildStatusText(summary));
                });
            } catch (Exception error) {
                String message = error.getMessage();
                if (message == null || message.isEmpty()) {
                    message = error.getClass().getSimpleName();
                }
                String summary = "Import failed: " + message;
                runOnUiThread(() -> {
                    importButton.setEnabled(true);
                    status.setText(buildStatusText(summary));
                });
            }
        }, "NeverGoneAssetImport").start();
    }

    private String buildStatusText(String notice) {
        StringBuilder text = new StringBuilder("Never Gone Recomp\n\n");
        if (notice != null && !notice.isEmpty()) {
            text.append(notice).append("\n\n");
        }

        File startLua = new File(getFilesDir(), "assets/Script/Game/StartLua.lua");
        text.append("Original assets: ")
                .append(startLua.isFile() ? "present" : "not imported")
                .append("\n\n")
                .append(nativeBootstrapInfo())
                .append("\n\nClient UI state\n")
                .append(nativeClientUiState());
        return text.toString();
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

    private String getAppVersion() {
        try {
            String value = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
            return value != null ? value : "";
        } catch (PackageManager.NameNotFoundException ignored) {
            return "";
        }
    }
}
