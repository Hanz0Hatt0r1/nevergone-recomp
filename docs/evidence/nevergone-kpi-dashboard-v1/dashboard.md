# Nevergone Frozen Analysis KPI Dashboard

**Status:** FAIL

Frozen evidence status for receiver coverage and the accept-router gate. Crossfit calibration is not used as a substitute for independent gate metrics.

| KPI | Actual | Target | Status | Source |
| --- | ---: | --- | --- | --- |
| Unseen receiver function coverage | 10.60% | ≥ 35.00% | FAIL | receiver_coverage |
| Frozen router accepted precision | unavailable | ≥ 97.00% | UNAVAILABLE | router_gate_availability |
| Frozen router coverage | unavailable | ≥ 55.00% | UNAVAILABLE | router_gate_availability |
| Critical false-accept rate | unavailable | ≤ 1.00% | UNAVAILABLE | router_gate_availability |
| Evidence-conflict accepts | unavailable | ≤ 0 | UNAVAILABLE | router_gate_availability |

- **Unseen receiver function coverage:** M2.6 hard target is 35%; current frozen report uses 500 identity-disjoint functions.

- **Frozen router accepted precision:** Unavailable until an independent labeled gate population is collected.

- **Frozen router coverage:** Unavailable until an independent labeled gate population is collected.

- **Critical false-accept rate:** Requires consumer-reviewed critical-case labels on the frozen gate population.

- **Evidence-conflict accepts:** Requires explicit evidence-conflict annotations on the frozen gate population.

## Sources

| ID | Format | SHA-256 |
| --- | --- | --- |
| receiver_coverage | REVENGINSTR.BenchmarkSummary/1 | `67f234b25315bc75e0718576eb5c0f52fe7e97cee333fc614bf821a7efc76477` |
| router_gate_availability | NEVERGONE.RouterGateAvailability/1 | `9b2565215f7df613a7600652b25fd30b855a4d3ace0b98dcef82302d9c9b446a` |

Unavailable metrics remain incomplete; they are not treated as passes.

Manifest SHA-256: `0f6c9e66c2c09796f2fc96cf9ceef1ef8273e2d970cd689146955f3543271e1f`
