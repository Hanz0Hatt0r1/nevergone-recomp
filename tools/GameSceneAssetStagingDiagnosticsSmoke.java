package org.nevergone.recomp;

public final class GameSceneAssetStagingDiagnosticsSmoke {
    public static void main(String[] args) {
        GameSceneAssetStagingDiagnostics.reset();
        String report = GameSceneAssetStagingDiagnostics.statusReport();
        assert report.contains("state: idle");
        assert report.contains("attempts: 0");

        GameSceneAssetStagingDiagnostics.begin(77L, 3);
        GameSceneAssetStagingDiagnostics.progress(2);
        report = GameSceneAssetStagingDiagnostics.statusReport();
        assert report.contains("state: staging");
        assert report.contains("attempts: 1");
        assert report.contains("revision: 77");
        assert report.contains("requests: 3");
        assert report.contains("staged: 2");

        GameSceneAssetStagingDiagnostics.failed(
                "frame-resolution-failed",
                77L,
                3,
                2,
                GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME,
                2);
        report = GameSceneAssetStagingDiagnostics.statusReport();
        assert report.contains("state: frame-resolution-failed");
        assert report.contains("failed request index: 2");
        assert report.contains("failed request kind: sprite-frame-by-name");
        assert !report.contains("hero.png");

        GameSceneAssetStagingDiagnostics.begin(88L, 1);
        GameSceneAssetStagingDiagnostics.succeeded(88L, 1);
        report = GameSceneAssetStagingDiagnostics.statusReport();
        assert report.contains("state: ready");
        assert report.contains("successes: 1");
        assert report.contains("staged: 1");
        assert !report.contains("failed request index:");

        GameSceneAssetStagingDiagnostics.noLiveQueue();
        report = GameSceneAssetStagingDiagnostics.statusReport();
        assert report.contains("state: no-live-queue");
        assert report.contains("revision: 0");
        assert report.contains("requests: 0");
        // Lifetime counters remain useful across scene transitions.
        assert report.contains("attempts: 2");
        assert report.contains("successes: 1");
    }
}
