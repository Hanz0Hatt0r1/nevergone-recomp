package org.nevergone.recomp;

/** Thread-safe GameScene pixel-staging diagnostics with bounded on-device request detail. */
final class GameSceneAssetStagingDiagnostics {
    private static final int MAX_DIAGNOSTIC_VALUE_LENGTH = 512;

    private static long attemptCount;
    private static long successCount;
    private static long observedRevision;
    private static int requestCount;
    private static int stagedCount;
    private static int failedRequestIndex = -1;
    private static int failedRequestKind = -1;
    private static String state = "idle";

    private static native String nativeTutorialBootstrapStatusReport();

    private GameSceneAssetStagingDiagnostics() {}

    static synchronized void reset() {
        attemptCount = 0L;
        successCount = 0L;
        observedRevision = 0L;
        requestCount = 0;
        stagedCount = 0;
        failedRequestIndex = -1;
        failedRequestKind = -1;
        state = "idle";
    }

    static synchronized void noLiveQueue() {
        observedRevision = 0L;
        requestCount = 0;
        stagedCount = 0;
        failedRequestIndex = -1;
        failedRequestKind = -1;
        state = "no-live-queue";
    }

    static synchronized void begin(long revision, int requests) {
        ++attemptCount;
        observedRevision = revision;
        requestCount = Math.max(0, requests);
        stagedCount = 0;
        failedRequestIndex = -1;
        failedRequestKind = -1;
        state = "staging";
    }

    static synchronized void progress(int staged) {
        stagedCount = Math.max(0, Math.min(staged, requestCount));
    }

    static synchronized void ready(long revision, int requests) {
        observedRevision = revision;
        requestCount = Math.max(0, requests);
        stagedCount = requestCount;
        failedRequestIndex = -1;
        failedRequestKind = -1;
        state = "ready";
    }

    static synchronized void succeeded(long revision, int requests) {
        ++successCount;
        ready(revision, requests);
    }

    static synchronized void failed(
            String reason,
            long revision,
            int requests,
            int requestIndex,
            int requestKind,
            int staged) {
        observedRevision = revision;
        requestCount = Math.max(0, requests);
        stagedCount = Math.max(0, Math.min(staged, requestCount));
        failedRequestIndex = requestIndex;
        failedRequestKind = requestKind;
        state = reason == null || reason.isEmpty() ? "unknown-failure" : reason;
    }

    static synchronized String statusReport() {
        StringBuilder out = new StringBuilder("GameScene asset staging\n");
        out.append("state: ").append(state).append('\n');
        out.append("attempts: ").append(attemptCount).append('\n');
        out.append("successes: ").append(successCount).append('\n');
        out.append("revision: ").append(observedRevision).append('\n');
        out.append("requests: ").append(requestCount).append('\n');
        out.append("staged: ").append(stagedCount).append('\n');
        if (failedRequestIndex >= 0) {
            out.append("failed request index: ").append(failedRequestIndex).append('\n');
        }
        if (failedRequestKind >= 0) {
            out.append("failed request kind: ")
                    .append(kindName(failedRequestKind))
                    .append('\n');
        }

        appendFailedRequestDetail(out);

        // Host-only Java smokes do not load the Android native library. Keep
        // the existing diagnostics independently testable there, while a real
        // Android process appends the native tutorial bootstrap/session state.
        try {
            String tutorial = nativeTutorialBootstrapStatusReport();
            if (tutorial != null && !tutorial.isEmpty()) {
                out.append(tutorial);
            }
        } catch (UnsatisfiedLinkError ignored) {
            // Expected outside the Android runtime.
        }
        return out.toString();
    }

    private static void appendFailedRequestDetail(StringBuilder out) {
        if (failedRequestIndex < 0) return;
        try {
            String value = GameSceneDirectAssetRequests.valueAt(failedRequestIndex);
            if (value != null) {
                out.append("failed request value: ")
                        .append(diagnosticValue(value))
                        .append('\n');
                out.append("failed request value length: ")
                        .append(value.length())
                        .append('\n');
            }

            boolean playerBody = GameSceneDirectAssetRequests.isPlayerBodyAt(failedRequestIndex);
            out.append("failed request player body: ")
                    .append(playerBody ? "yes" : "no")
                    .append('\n');
            if (playerBody) {
                String atlas = GameSceneDirectAssetRequests.playerAtlasPlistAt(failedRequestIndex);
                if (atlas != null) {
                    out.append("failed request player atlas: ")
                            .append(diagnosticValue(atlas))
                            .append('\n');
                }
            }
        } catch (UnsatisfiedLinkError ignored) {
            // Expected in host-side Java smoke tests.
        }
    }

    private static String diagnosticValue(String value) {
        int length = Math.min(value.length(), MAX_DIAGNOSTIC_VALUE_LENGTH);
        StringBuilder safe = new StringBuilder(length + 3);
        for (int index = 0; index < length; ++index) {
            char c = value.charAt(index);
            safe.append(c >= 0x20 && c != 0x7f ? c : '?');
        }
        if (value.length() > length) safe.append("...");
        return safe.toString();
    }

    private static String kindName(int kind) {
        if (kind == GameSceneDirectAssetRequests.KIND_DIRECT_FILE) return "direct-file";
        if (kind == GameSceneDirectAssetRequests.KIND_SPRITE_FRAME_BY_NAME) {
            return "sprite-frame-by-name";
        }
        return "unknown";
    }
}
