#!/usr/bin/env python3
from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUILD = HERE / "build_nevergone_conditional_fusion_candidate.py"


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row, sort_keys=True) + "\n" for row in rows), encoding="utf-8")


def fixture():
    readiness = {
        "format": "NEVERGONE.ConditionalFusionReadiness/1",
        "task": "code_domain",
        "scope": "per_label_action",
        "functions": 30,
        "trusted_transfer_classes": ["engine", "media_library", "runtime_library"],
        "eligible_policy_keys": ["engine/review"],
        "ready_for_independent_validation": True,
        "production_routing_changed": False,
    }
    production = {
        "format": "NEVERGONE.ProductionClassifier/4",
        "task": "code_domain",
        "functions": 30,
        "families": 6,
        "dataset_fingerprint_sha256": "dev-fingerprint",
    }
    transfer_manifest = {
        "format": "NEVERGONE.SimilarityTransferOOF/1",
        "task": "code_domain",
        "functions": 30,
        "families": 6,
        "dataset_fingerprint_sha256": "dev-fingerprint",
    }
    classifier = []
    transfer = []
    for index in range(30):
        identity = f"f{index:02d}"
        family = f"fam{index // 5}"
        action = "review" if index < 15 else "accept"
        classifier.append({
            "id": identity,
            "family_id": family,
            "truth": "engine",
            "prediction": "engine",
            "action": action,
        })
        transfer.append({
            "id": identity,
            "family_id": family,
            "truth": "engine",
            "prediction": "engine",
            "correct": True,
            "similarity": 0.96 if index < 15 else 0.91,
            "gap": 0.20 if index < 15 else 0.10,
            "nearest_family": "reference-engine",
        })
    return readiness, production, classifier, transfer_manifest, transfer


def run(values, *, good: bool, extra: list[str] | None = None):
    readiness, production, classifier, transfer_manifest, transfer = values
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        r = root / "readiness.json"
        p = root / "production.json"
        coof = root / "classifier.jsonl"
        tm = root / "transfer-manifest.json"
        toof = root / "transfer.jsonl"
        out = root / "candidate.json"
        write_json(r, readiness)
        write_json(p, production)
        write_jsonl(coof, classifier)
        write_json(tm, transfer_manifest)
        write_jsonl(toof, transfer)
        cmd = [
            sys.executable,
            str(BUILD),
            str(r), str(p), str(coof), str(tm), str(toof),
            "--output", str(out),
        ]
        if extra:
            cmd.extend(extra)
        proc = subprocess.run(cmd, capture_output=True, text=True)
        if (proc.returncode == 0) != good:
            raise AssertionError((proc.returncode, proc.stdout, proc.stderr))
        data = json.loads(out.read_text()) if out.is_file() else None
        return proc, data


def main() -> int:
    base = fixture()
    proc, data = run(base, good=True)
    assert data is not None
    assert data["format"] == "NEVERGONE.ConditionalFusionCandidate/1"
    assert data["eligible_policy_keys"] == ["engine/review"]
    assert data["conditional_policy"]["policies"]["engine/review"]["enabled"] is True
    assert data["conditional_policy"]["policies"]["engine/review"]["examples"] >= 10
    assert data["development_oof"]["promoted_examples"] >= 10
    assert data["development_oof"]["promoted_precision"] == 1.0
    assert data["validation_state"] == "pending_independent_validation"
    assert data["production_routing_changed"] is False

    values = list(copy.deepcopy(base))
    values[0]["ready_for_independent_validation"] = False
    run(tuple(values), good=False)

    values = list(copy.deepcopy(base))
    values[3]["dataset_fingerprint_sha256"] = "other"
    run(tuple(values), good=False)

    values = list(copy.deepcopy(base))
    values[4][0]["family_id"] = "wrong-family"
    run(tuple(values), good=False)

    values = list(copy.deepcopy(base))
    values[4] = values[4][:9] + [row for row in values[4][15:]]
    run(tuple(values), good=False)

    values = list(copy.deepcopy(base))
    values[0]["eligible_policy_keys"] = ["game/review"]
    run(tuple(values), good=False)

    # Exact eligible key must remain supportable when frozen on full-development OOF.
    values = list(copy.deepcopy(base))
    for row in values[4]:
        if int(row["id"][1:]) < 15:
            row["correct"] = False
            row["prediction"] = "media_library"
    run(tuple(values), good=False)

    print("conditional fusion candidate smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
