#!/usr/bin/env python3
"""Objective function-origin routing helpers for Nevergone ML tooling.

These helpers intentionally use source metadata only outside the semantic model.
They identify functions that should not be presented to the code-domain model at
all, such as imported external symbols or ELF init/fini scaffolding.
"""
from __future__ import annotations

import re

_RUNTIME_SCAFFOLD_RE = re.compile(r"^_(?:INIT|FINI)(?:_|$)", re.IGNORECASE)


def classify_function_origin(source_name: str | None) -> str:
    """Return internal, external_import, or runtime_scaffold."""
    if not isinstance(source_name, str) or not source_name:
        return "internal"
    name = source_name.strip()
    if name.startswith("<EXTERNAL>::") or name == "<EXTERNAL>":
        return "external_import"
    # Keep this deliberately narrow so ordinary C++ methods named init/fini are
    # not excluded. This targets linker-generated names such as _FINI_0.
    if "::" not in name and _RUNTIME_SCAFFOLD_RE.match(name):
        return "runtime_scaffold"
    return "internal"


def is_semantic_model_candidate(source_name: str | None) -> bool:
    return classify_function_origin(source_name) == "internal"
