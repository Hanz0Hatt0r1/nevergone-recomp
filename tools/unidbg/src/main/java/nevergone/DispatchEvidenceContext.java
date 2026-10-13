package nevergone;

import com.alibaba.fastjson.JSONObject;

/**
 * Process-local evidence origin for bounded dispatch probes.
 *
 * The unidbg harness executes one native workload at a time. Natural dispatch
 * observations are eligible for verified natural seeds. Once a bounded fallback
 * control mutates R0 after the first virtual return, subsequent dispatches in that
 * workload are still valid runtime target/vtable observations, but their path
 * reachability is interventional and must not be promoted into natural seeds.
 */
final class DispatchEvidenceContext {
    static final String NATURAL = "natural_runtime";
    static final String INTERVENTIONAL_FALLBACK = "interventional_fallback";

    private static String evidenceOrigin = NATURAL;
    private static String interventionSourceId;

    static synchronized void reset() {
        evidenceOrigin = NATURAL;
        interventionSourceId = null;
    }

    static synchronized void markInterventionalFallback(String sourceId) {
        if (sourceId == null || sourceId.isEmpty())
            throw new IllegalArgumentException("Interventional dispatch source_id is required");
        evidenceOrigin = INTERVENTIONAL_FALLBACK;
        interventionSourceId = sourceId;
    }

    static synchronized void annotate(JSONObject row) {
        boolean natural = NATURAL.equals(evidenceOrigin);
        row.put("evidence_origin", evidenceOrigin);
        row.put("eligible_for_natural_seed", natural);
        row.put("natural_path_reachability_verified", natural);
        row.put("intervention_kind", natural ? null : "forced_r0_zero_after_first_virtual_call_return");
        row.put("intervention_source_id", natural ? null : interventionSourceId);
    }

    private DispatchEvidenceContext() {}
}
