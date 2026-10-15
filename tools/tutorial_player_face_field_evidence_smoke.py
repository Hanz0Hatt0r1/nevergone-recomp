#!/usr/bin/env python3

from __future__ import annotations

from tutorial_player_face_field_evidence import render, resolve


def accessor(offset: str = "0x12c") -> list[dict[str, str]]:
    return [{
        "format": "NEVERGONE.GhidraAccessorReturnField/1",
        "status": "return_load_from_parameter_constant_offset",
        "load_base_parameter_index": "0",
        "load_offset": offset,
    }]


def store(offset: str = "0x130") -> list[dict[str, str]]:
    return [{
        "format": "NEVERGONE.ParameterStoreField/1",
        "status": "parameter_to_parameter_field",
        "destination_parameter_index": "0",
        "source_parameter_index": "1",
        "destination_offset": offset,
    }]


def expect_error(a, b, needle: str) -> None:
    try:
        resolve(a, b)
    except ValueError as error:
        assert needle in str(error), (needle, error)
        return
    raise AssertionError("expected ValueError")


def main() -> int:
    evidence = resolve(accessor(), store())
    assert evidence.is_face_offset == 0x12C
    assert evidence.player_face_dir_offset == 0x130
    report = render(evidence)
    assert "MEPlayer::isFace field candidate: 0x12c" in report
    assert "MEPlayer::setPlayerDir field candidate: 0x130" in report
    assert "fields distinct: true" in report
    assert "structural field candidates only" in report

    duplicate = accessor() + accessor("0x131")
    expect_error(duplicate, store(), "is not unique")
    duplicate_store = store() + store("0x134")
    expect_error(accessor(), duplicate_store, "is not unique")

    wrong_source = store()
    wrong_source[0]["source_parameter_index"] = "2"
    expect_error(accessor(), wrong_source, "none")

    print("Tutorial player face field evidence smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
