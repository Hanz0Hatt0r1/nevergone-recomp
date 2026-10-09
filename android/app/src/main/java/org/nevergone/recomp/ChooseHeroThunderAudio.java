package org.nevergone.recomp;

import android.media.AudioAttributes;
import android.media.SoundPool;

import java.io.File;
import java.util.Arrays;

final class ChooseHeroThunderAudio {
    private static final String[] RELATIVE_PATHS = {
            "sound/SingleLogin_UI/L_thunder08-r.mp3",
            "sound/SingleLogin_UI/L_Thunder09.mp3",
            "sound/SingleLogin_UI/L_Thunder10.mp3",
            "sound/SingleLogin_UI/M_thunder_norm_2.mp3",
            "sound/SingleLogin_UI/M_thunder_norm_5.mp3",
            "sound/SingleLogin_UI/M_Thunder04.mp3",
            "sound/SingleLogin_UI/S_thunder_norm_1.mp3"
    };

    private final File assetRoot;
    private final int[] soundIds = new int[RELATIVE_PATHS.length];
    private final boolean[] ready = new boolean[RELATIVE_PATHS.length];
    private SoundPool soundPool;
    private boolean appResumed = true;
    private String status = "waiting for imported ChooseHero thunder SFX";

    ChooseHeroThunderAudio(File assetRoot) {
        this.assetRoot = assetRoot;
    }

    synchronized void onAssetsReloaded() {
        releasePool("reloading imported ChooseHero thunder SFX");
        if (appResumed) loadPool();
    }

    synchronized void onPause() {
        appResumed = false;
        if (soundPool != null) soundPool.autoPause();
    }

    synchronized void onResume() {
        appResumed = true;
        if (soundPool == null) loadPool();
        if (soundPool != null) soundPool.autoResume();
    }

    synchronized void play(int soundIndex) {
        if (!appResumed || soundPool == null ||
                soundIndex < 0 || soundIndex >= soundIds.length || !ready[soundIndex]) {
            return;
        }
        soundPool.play(soundIds[soundIndex], 1.0f, 1.0f, 1, 0, 1.0f);
    }

    synchronized void release() {
        appResumed = false;
        releasePool("released");
    }

    synchronized String status() {
        return status;
    }

    private void loadPool() {
        releasePool("loading imported ChooseHero thunder SFX");
        AudioAttributes attributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build();
        SoundPool created = new SoundPool.Builder()
                .setMaxStreams(4)
                .setAudioAttributes(attributes)
                .build();
        soundPool = created;
        Arrays.fill(soundIds, 0);
        Arrays.fill(ready, false);

        created.setOnLoadCompleteListener((pool, sampleId, loadStatus) -> {
            synchronized (ChooseHeroThunderAudio.this) {
                if (soundPool != pool) return;
                for (int index = 0; index < soundIds.length; index++) {
                    if (soundIds[index] == sampleId) {
                        ready[index] = loadStatus == 0;
                        break;
                    }
                }
                updateStatus();
            }
        });

        int requested = 0;
        for (int index = 0; index < RELATIVE_PATHS.length; index++) {
            File source = new File(assetRoot, RELATIVE_PATHS[index]);
            if (!source.isFile()) continue;
            try {
                soundIds[index] = created.load(source.getAbsolutePath(), 1);
                if (soundIds[index] != 0) requested++;
            } catch (RuntimeException ignored) {
                soundIds[index] = 0;
            }
        }
        if (requested == 0) {
            releasePool("ChooseHero thunder assets not imported");
        } else {
            status = "loading " + requested + "/" + RELATIVE_PATHS.length;
        }
    }

    private void updateStatus() {
        int requested = 0;
        int loaded = 0;
        for (int index = 0; index < soundIds.length; index++) {
            if (soundIds[index] != 0) {
                requested++;
                if (ready[index]) loaded++;
            }
        }
        status = "loaded " + loaded + "/" + requested + " ChooseHero thunder SFX";
    }

    private void releasePool(String newStatus) {
        if (soundPool != null) {
            soundPool.setOnLoadCompleteListener(null);
            soundPool.release();
            soundPool = null;
        }
        Arrays.fill(soundIds, 0);
        Arrays.fill(ready, false);
        status = newStatus;
    }
}
