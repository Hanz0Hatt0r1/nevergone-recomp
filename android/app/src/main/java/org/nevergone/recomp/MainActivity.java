package org.nevergone.recomp;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
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
    private static final int REQUEST_ORIGINAL_OBB = 1002;
    private static final long CHOOSE_HERO_THUNDER_POLL_MS = 16L;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private TextView status;
    private Button importButton;
    private Button importObbButton;
    private GameSurfaceView gameSurface;
    private ChooseHeroThunderAudio chooseHeroThunderAudio;
    private boolean chooseHeroThunderPolling;

    private final Runnable chooseHeroThunderPoll = new Runnable() {
        @Override
        public void run() {
            if (!chooseHeroThunderPolling) return;
            if (chooseHeroThunderAudio != null) {
                for (int soundIndex = nativePollChooseHeroThunderSound();
                        soundIndex >= 0;
                        soundIndex = nativePollChooseHeroThunderSound()) {
                    chooseHeroThunderAudio.play(soundIndex);
                }
            }
            if (chooseHeroThunderPolling) {
                mainHandler.postDelayed(this, CHOOSE_HERO_THUNDER_POLL_MS);
            }
        }
    };

    static {
        System.loadLibrary("nevergone_recomp");
    }

    private static native void nativeConfigureRuntime(String filesDir, String deviceId, String appVersion);
    private static native String nativeBootstrapInfo();
    private static native String nativeClientUiState();
    private static native void nativeOnAppPause();
    private static native void nativeOnAppResume();
    private static native void nativeAppDelegateOnPause();
    private static native void nativeAppDelegateOnResume();
    private static native int nativePollChooseHeroThunderSound();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        nativeConfigureRuntime(
                getFilesDir().getAbsolutePath(),
                getOrCreateDeviceId(),
                getAppVersion());
        chooseHeroThunderAudio = new ChooseHeroThunderAudio(
                new File(getFilesDir(), "assets"));

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

        importObbButton = new Button(this);
        importObbButton.setText("Import original Never Gone OBB");
        importObbButton.setOnClickListener(view -> chooseOriginalObb());
        content.addView(importObbButton, new LinearLayout.LayoutParams(
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
        stopChooseHeroThunderPolling();
        if (chooseHeroThunderAudio != null) {
            chooseHeroThunderAudio.onPause();
        }
        if (gameSurface != null) {
            gameSurface.pauseImportedAudio();
        }
        nativeAppDelegateOnPause();
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
        nativeAppDelegateOnResume();
        if (gameSurface != null) {
            gameSurface.resumeImportedAudio();
        }
        if (chooseHeroThunderAudio != null) {
            chooseHeroThunderAudio.onResume();
        }
        startChooseHeroThunderPolling();
    }

    @Override
    protected void onDestroy() {
        stopChooseHeroThunderPolling();
        if (chooseHeroThunderAudio != null) {
            chooseHeroThunderAudio.release();
        }
        if (gameSurface != null) {
            gameSurface.releaseImportedAudio();
        }
        super.onDestroy();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null) return;

        Uri uri = data.getData();
        if (uri == null) {
            status.setText(buildStatusText("Import failed: no file selected."));
            return;
        }
        if (requestCode == REQUEST_ORIGINAL_APK) {
            importOriginalApk(uri);
        } else if (requestCode == REQUEST_ORIGINAL_OBB) {
            importOriginalObb(uri);
        }
    }

    private void chooseOriginalApk() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/vnd.android.package-archive");
        startActivityForResult(intent, REQUEST_ORIGINAL_APK);
    }

    private void chooseOriginalObb() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, REQUEST_ORIGINAL_OBB);
    }

    private void setImportButtonsEnabled(boolean enabled) {
        importButton.setEnabled(enabled);
        importObbButton.setEnabled(enabled);
    }

    private void importOriginalApk(Uri apkUri) {
        setImportButtonsEnabled(false);
        status.setText(buildStatusText("Importing and decoding original APK assets..."));

        new Thread(() -> {
            try {
                OriginalApkImporter.Result result = OriginalApkImporter.importAssets(
                        getContentResolver(), apkUri, getFilesDir());
                String summary = String.format(
                        Locale.ROOT,
                        "Imported %d APK files (%.2f MiB); decoded %d encoded assets. " +
                                "Import the original OBB next for expansion/game-scene resources.",
                        result.files,
                        result.bytes / (1024.0 * 1024.0),
                        result.decodedFiles);
                runOnUiThread(() -> {
                    setImportButtonsEnabled(true);
                    if (gameSurface != null) gameSurface.reloadImportedSplash();
                    if (chooseHeroThunderAudio != null) chooseHeroThunderAudio.onAssetsReloaded();
                    status.setText(buildStatusText(summary));
                });
            } catch (Exception error) {
                showImportError(error);
            }
        }, "NeverGoneAssetImport").start();
    }

    private void importOriginalObb(Uri obbUri) {
        setImportButtonsEnabled(false);
        status.setText(buildStatusText("Importing and decoding original OBB assets..."));

        new Thread(() -> {
            try {
                OriginalObbImporter.Result result = OriginalObbImporter.importAssets(
                        getContentResolver(), obbUri, getFilesDir());
                String summary = String.format(
                        Locale.ROOT,
                        "Imported %d OBB files (%.2f MiB); decoded %d encoded assets. " +
                                "Expansion resources are now available to the reconstructed runtime.",
                        result.files,
                        result.bytes / (1024.0 * 1024.0),
                        result.decodedFiles);
                runOnUiThread(() -> {
                    setImportButtonsEnabled(true);
                    if (gameSurface != null) gameSurface.reloadImportedSplash();
                    if (chooseHeroThunderAudio != null) chooseHeroThunderAudio.onAssetsReloaded();
                    status.setText(buildStatusText(summary));
                });
            } catch (Exception error) {
                showImportError(error);
            }
        }, "NeverGoneObbImport").start();
    }

    private void showImportError(Exception error) {
        String message = error.getMessage();
        if (message == null || message.isEmpty()) {
            message = error.getClass().getSimpleName();
        }
        final String summary = "Import failed: " + message;
        runOnUiThread(() -> {
            setImportButtonsEnabled(true);
            status.setText(buildStatusText(summary));
        });
    }

    private void startChooseHeroThunderPolling() {
        if (chooseHeroThunderPolling || chooseHeroThunderAudio == null) return;
        chooseHeroThunderPolling = true;
        mainHandler.post(chooseHeroThunderPoll);
    }

    private void stopChooseHeroThunderPolling() {
        if (!chooseHeroThunderPolling) return;
        chooseHeroThunderPolling = false;
        mainHandler.removeCallbacks(chooseHeroThunderPoll);
    }

    private String buildStatusText(String notice) {
        StringBuilder text = new StringBuilder("Never Gone Recomp\n\n");
        if (notice != null && !notice.isEmpty()) {
            text.append(notice).append("\n\n");
        }

        File startLua = new File(getFilesDir(), "assets/Script/Game/StartLua.lua");
        File pvpScene = new File(getFilesDir(), "assets/gamescene/gs_list/pvp_scene.glData");
        File chooseHeroAtlas = new File(
                getFilesDir(),
                "assets/gamescene_ui/LevelUI/Gate_Background_UI/Gate_BackgroundPNG_01.plist");
        text.append("Original APK assets: ")
                .append(startLua.isFile() ? "present" : "not imported")
                .append("\n")
                .append("Original OBB assets: ")
                .append(pvpScene.isFile() && chooseHeroAtlas.isFile() ? "present" : "not imported")
                .append("\n");
        if (gameSurface != null) {
            text.append("SingleLogin BGM: ")
                    .append(gameSurface.importedAudioStatus())
                    .append("\n");
        }
        if (chooseHeroThunderAudio != null) {
            text.append("ChooseHero thunder: ")
                    .append(chooseHeroThunderAudio.status())
                    .append("\n");
        }
        text.append("\n")
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
