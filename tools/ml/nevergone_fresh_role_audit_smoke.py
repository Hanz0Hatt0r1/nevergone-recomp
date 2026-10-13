#!/usr/bin/env python3
from __future__ import annotations

from augment_nevergone_function_roles import infer_function_role


def main() -> int:
    assert infer_function_role("Foo::getValue()") == (
        "getter",
        "get/is/has-style recovered method name",
    )
    assert infer_function_role("Foo::setValue(int)") == (
        "setter",
        "setX-style recovered method name",
    )
    assert infer_function_role("Foo::~Foo()") == (
        "destructor",
        "qualified C++ destructor name",
    )
    assert infer_function_role("Foo::onEnter()") == (
        "callback",
        "onX-style recovered method name",
    )
    assert infer_function_role("FUN_00100000") is None
    print("nevergone fresh role audit smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
