#!/usr/bin/env python3
from __future__ import annotations

from classify_nevergone_hierarchy_with_role import apply_role_policy, attach_roles


def hierarchy_row(origin: str = "internal"):
    return {
        "format": "NEVERGONE.HierarchicalFunctionRouter/1",
        "id": "x",
        "function_origin": {
            "prediction": origin,
            "confidence": 1.0,
            "action": "accept",
        },
        "code_domain": {"prediction": "engine", "action": "review"} if origin == "internal" else None,
        "subsystem": None,
        "final_action": "review" if origin == "internal" else "accept",
        "semantic_models_invoked": ["code_domain"] if origin == "internal" else [],
    }


def role(prediction: str, action: str = "accept"):
    return {
        "task": "function_role",
        "prediction": prediction,
        "action": action,
        "confidence": 0.99,
    }


def main() -> int:
    callback = apply_role_policy(role("callback"), allowed_accept_roles=set())
    assert callback["model_action"] == "accept"
    assert callback["action"] == "review"
    assert callback["production_guard"] == "function_role_accept_not_validated"

    getter = apply_role_policy(role("getter"), allowed_accept_roles=set())
    assert getter["model_action"] == "accept"
    assert getter["action"] == "review"
    assert getter["production_guard"] == "function_role_accept_not_validated"

    explicit_callback = apply_role_policy(role("callback"), allowed_accept_roles={"callback"})
    assert explicit_callback["action"] == "accept"
    assert "production_guard" not in explicit_callback

    reviewed = apply_role_policy(role("getter", "review"), allowed_accept_roles=set())
    assert reviewed["action"] == "review"
    assert "model_action" not in reviewed

    source_rows = [{"id": "a"}, {"id": "b"}]
    hierarchy_rows = [hierarchy_row("internal"), hierarchy_row("external_import")]
    attach_roles(
        source_rows=source_rows,
        hierarchy_rows=hierarchy_rows,
        role_rows=[role("callback")],
        allowed_accept_roles=set(),
    )

    internal = hierarchy_rows[0]
    external = hierarchy_rows[1]
    assert internal["function_role"]["prediction"] == "callback"
    assert internal["function_role"]["model_action"] == "accept"
    assert internal["function_role"]["action"] == "review"
    assert internal["final_action"] == "review"
    assert internal["semantic_models_invoked"] == ["code_domain", "function_role"]
    assert external["function_role"] is None
    assert external["final_action"] == "accept"

    print("nevergone hierarchy with role smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
