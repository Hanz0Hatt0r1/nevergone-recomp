#!/usr/bin/env python3
from __future__ import annotations

from classify_nevergone_hierarchy import attach_subsystem, internal_base_result


def row():
    return {
        "program": "libcocos2dcpp.so",
        "source_entry": "00100000",
        "source_name": "GameScene::update(float)",
    }


def main() -> int:
    engine = internal_base_result(
        row(),
        0,
        {
            "task": "code_domain",
            "prediction": "engine",
            "action": "accept",
            "confidence": 0.99,
        },
    )
    assert engine["subsystem"] is None
    assert engine["final_action"] == "accept"
    assert engine["routing_reason"] == "code_domain_terminal"
    assert engine["semantic_models_invoked"] == ["code_domain"]

    game_review = internal_base_result(
        row(),
        0,
        {
            "task": "code_domain",
            "prediction": "game",
            "action": "review",
            "confidence": 0.8,
        },
    )
    assert game_review["routing_reason"] == "code_domain_game_not_accepted"
    assert game_review["subsystem"] is None

    game_accept = internal_base_result(
        row(),
        0,
        {
            "task": "code_domain",
            "prediction": "game",
            "action": "accept",
            "confidence": 0.99,
        },
    )
    assert game_accept["routing_reason"] == "subsystem_pending"

    attach_subsystem(
        game_accept,
        {
            "task": "subsystem",
            "prediction": "combat",
            "action": "accept",
            "confidence": 0.99,
        },
        blocked_accept_labels={"combat"},
    )
    assert game_accept["subsystem"]["model_action"] == "accept"
    assert game_accept["subsystem"]["action"] == "review"
    assert game_accept["subsystem"]["production_guard"] == "subsystem_accept_blocked"
    assert game_accept["final_action"] == "review"
    assert game_accept["semantic_models_invoked"] == ["code_domain", "subsystem"]

    scene = internal_base_result(
        row(),
        0,
        {
            "task": "code_domain",
            "prediction": "game",
            "action": "accept",
            "confidence": 0.99,
        },
    )
    attach_subsystem(
        scene,
        {
            "task": "subsystem",
            "prediction": "scene",
            "action": "accept",
            "confidence": 0.98,
        },
        blocked_accept_labels={"combat"},
    )
    assert scene["final_action"] == "accept"
    assert scene["subsystem"]["action"] == "accept"
    assert "production_guard" not in scene["subsystem"]

    print("nevergone hierarchical router smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
