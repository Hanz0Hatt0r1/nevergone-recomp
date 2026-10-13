#!/usr/bin/env python3
from __future__ import annotations

from nevergone_function_origin import classify_function_origin, is_semantic_model_candidate


def main() -> int:
    assert classify_function_origin("<EXTERNAL>::cos") == "external_import"
    assert classify_function_origin("<EXTERNAL>") == "external_import"
    assert classify_function_origin("_FINI_0") == "runtime_scaffold"
    assert classify_function_origin("_INIT_1") == "runtime_scaffold"
    assert classify_function_origin("GameScene::init") == "internal"
    assert classify_function_origin("FUN_00289580") == "internal"
    assert is_semantic_model_candidate("EnemyActions::update")
    assert not is_semantic_model_candidate("<EXTERNAL>::sqrt")
    print("function origin smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
