package org.nevergone.recomp;

import android.media.MediaPlayer;

import java.io.File;

final class StartupLogoAudio {
    private static final String RELATIVE_PATH = "sound/Load/Logo_finVer.mp3";

    private final File assetRoot;
    private MediaPlayer player;
    private boolean appResumed = true;
    private boolean due;
    private boolean played;
    private String status = "waiting for splash sound cue";

    StartupLogoAudio(File assetRoot) {
        this.assetRoot = assetRoot;
    }

    synchronized void resetSequence() {
        due = false;
        played = false;
        releasePlayer("waiting for splash sound cue");
    }

    synchronized void setDue(boolean value) {
        due = value;
        if (!due || played || player != null) return;
        ensurePlayer();
    }

    synchronized void onAssetsReloaded() {
        if (!due || played) return;
        releasePlayer("reloading imported splash sound");
        ensurePlayer();
    }

    synchronized void onPause() {
        appResumed = false;
        if (player != null && player.isPlaying()) {
            player.pause();
            status = "paused";
        }
    }

    synchronized void onResume() {
        appResumed = true;
        if (player != null) {
            tryStart();
        } else if (due && !played) {
            ensurePlayer();
        }
    }

    synchronized void release() {
        releasePlayer("released");
    }

    synchronized String status() {
        return status;
    }

    private void ensurePlayer() {
        File source = new File(assetRoot, RELATIVE_PATH);
        if (!source.isFile()) {
            status = "splash sound asset not imported";
            return;
        }

        MediaPlayer created = new MediaPlayer();
        try {
            created.setDataSource(source.getAbsolutePath());
            created.setLooping(false);
            created.setOnPreparedListener(mediaPlayer -> {
                synchronized (StartupLogoAudio.this) {
                    if (player != mediaPlayer || played || !due) {
                        mediaPlayer.release();
                        if (player == mediaPlayer) player = null;
                        return;
                    }
                    status = "prepared";
                    tryStart();
                }
            });
            created.setOnCompletionListener(mediaPlayer -> {
                synchronized (StartupLogoAudio.this) {
                    if (player == mediaPlayer) {
                        status = "played";
                        player = null;
                    }
                    mediaPlayer.release();
                }
            });
            created.setOnErrorListener((mediaPlayer, what, extra) -> {
                synchronized (StartupLogoAudio.this) {
                    if (player == mediaPlayer) {
                        status = "error " + what + "/" + extra;
                        player = null;
                    }
                    mediaPlayer.release();
                }
                return true;
            });
            player = created;
            status = "preparing";
            created.prepareAsync();
        } catch (Exception error) {
            created.release();
            player = null;
            String message = error.getMessage();
            status = "error: " + (message == null ? error.getClass().getSimpleName() : message);
        }
    }

    private void tryStart() {
        if (player == null || played || !due || !appResumed) return;
        try {
            if (!player.isPlaying()) {
                player.start();
                played = true;
            }
            status = "playing once";
        } catch (IllegalStateException ignored) {
            // prepareAsync has not completed yet; the prepared listener retries.
        }
    }

    private void releasePlayer(String newStatus) {
        if (player != null) {
            player.setOnPreparedListener(null);
            player.setOnCompletionListener(null);
            player.setOnErrorListener(null);
            player.release();
            player = null;
        }
        status = newStatus;
    }
}
