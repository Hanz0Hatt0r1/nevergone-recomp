package org.nevergone.recomp;

import android.media.MediaPlayer;

import java.io.File;

final class SingleLoginAudio {
    private static final String RELATIVE_PATH = "sound/SingleLogin_Bgm.mp3";

    private final File assetRoot;
    private MediaPlayer player;
    private boolean appResumed = true;
    private boolean sceneActive = false;
    private String status = "waiting for SingleLogin scene";

    SingleLoginAudio(File assetRoot) {
        this.assetRoot = assetRoot;
    }

    synchronized void setSceneActive(boolean active) {
        if (sceneActive == active) {
            return;
        }
        sceneActive = active;
        if (!active) {
            releasePlayer("waiting for SingleLogin scene");
            return;
        }
        ensurePlayer();
    }

    synchronized void onAssetsReloaded() {
        if (!sceneActive) {
            return;
        }
        releasePlayer("reloading imported BGM");
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
        if (sceneActive) {
            if (player == null) {
                ensurePlayer();
            } else {
                tryStart();
            }
        }
    }

    synchronized void release() {
        sceneActive = false;
        releasePlayer("released");
    }

    synchronized String status() {
        return status;
    }

    private void ensurePlayer() {
        File source = new File(assetRoot, RELATIVE_PATH);
        if (!source.isFile()) {
            status = "BGM asset not imported";
            return;
        }

        MediaPlayer created = new MediaPlayer();
        try {
            created.setDataSource(source.getAbsolutePath());
            created.setLooping(true);
            created.setOnPreparedListener(mediaPlayer -> {
                synchronized (SingleLoginAudio.this) {
                    if (player != mediaPlayer || !sceneActive) {
                        mediaPlayer.release();
                        if (player == mediaPlayer) {
                            player = null;
                        }
                        return;
                    }
                    status = "prepared";
                    tryStart();
                }
            });
            created.setOnErrorListener((mediaPlayer, what, extra) -> {
                synchronized (SingleLoginAudio.this) {
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
        if (player == null || !sceneActive || !appResumed) {
            return;
        }
        try {
            if (!player.isPlaying()) {
                player.start();
            }
            status = "playing (loop)";
        } catch (IllegalStateException error) {
            // prepareAsync has not completed yet; the prepared listener will retry.
        }
    }

    private void releasePlayer(String newStatus) {
        if (player != null) {
            player.setOnPreparedListener(null);
            player.setOnErrorListener(null);
            player.release();
            player = null;
        }
        status = newStatus;
    }
}
