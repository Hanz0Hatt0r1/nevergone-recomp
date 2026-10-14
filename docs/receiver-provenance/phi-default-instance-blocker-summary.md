# PHI default-instance recovery blocker summary

The v7 PHI default-instance finalizer is deliberately fail-closed. When real-data recovery remains unresolved, use:

```bash
bash tools/ml/run_nevergone_phi_default_instance_blocker_summary.sh
```

The summary reads `phi-default-instance-recovery-v7.jsonl` and reports per-row gate failures plus aggregate reason counts. It is diagnostic only and does not alter receiver inference, query emission, or routing.
