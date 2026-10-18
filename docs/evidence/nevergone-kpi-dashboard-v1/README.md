# Nevergone frozen analysis KPI dashboard

This consumer-owned dashboard joins the frozen receiver benchmark with the
router gate availability report. Receiver function coverage is below the M2.6
target. The router precision, coverage, critical error, and conflict KPIs remain
unavailable because the existing crossfit result is not an independent gate.

The JSON and Markdown outputs contain source hashes and aggregate metrics only.
Recreate them with the pinned shared dashboard tool:

```bash
PYTHONPATH=revenginstr/src python -m revenginstr.kpi_dashboard \
  docs/evidence/nevergone-kpi-dashboard-v1/manifest.json \
  --json-output docs/evidence/nevergone-kpi-dashboard-v1/dashboard.json \
  --markdown-output docs/evidence/nevergone-kpi-dashboard-v1/dashboard.md
```
