#!/usr/bin/env python3
"""Analyze the unique EnemyObject::initActionsSystem -> createWithFile call site."""

from __future__ import annotations

import argparse
import csv
import re
from collections import defaultdict
from pathlib import Path

PCODE_REQUIRED = {
    "function_entry",
    "op_address",
    "op_time",
    "opcode",
    "output",
    "inputs",
    "callee_entry",
    "callee_name",
}
SUBTREE_FORMAT = "NEVERGONE.GhidraCallerArgumentSubtree/1"
IDENTITY_FORMAT = "NEVERGONE.GhidraFunctionIdentity/1"
PARAM_RE = re.compile(r"^enemy-actions-create-with-file:param\[(\d+)\]$")
TRANSPARENT_OPS = {"COPY", "CAST", "INDIRECT", "INT_ZEXT", "INT_SEXT", "SUBPIECE"}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if reader.fieldnames is None:
            raise ValueError(f"{path}: missing TSV header")
        return [dict(row) for row in reader]


def split_inputs(cell: str) -> tuple[str, ...]:
    text = (cell or "").strip()
    if text in ("", "-"):
        return ()
    return tuple(token for token in text.split(";") if token and token != "-")


def constant_value(token: str) -> int | None:
    if not token.startswith("const:0x"):
        return None
    try:
        return int(token.split(":", 2)[1], 16)
    except (IndexError, ValueError):
        return None


def combine_affine(
    left: tuple[dict[str, int], int] | None,
    right: tuple[dict[str, int], int] | None,
    sign: int,
) -> tuple[dict[str, int], int] | None:
    if left is None or right is None:
        return None
    terms = dict(left[0])
    for token, coefficient in right[0].items():
        terms[token] = terms.get(token, 0) + sign * coefficient
        if terms[token] == 0:
            del terms[token]
    return terms, left[1] + sign * right[1]


def format_affine(value: tuple[dict[str, int], int] | None) -> str:
    if value is None:
        return "-"
    terms, constant = value
    pieces: list[str] = []
    for token, coefficient in sorted(terms.items()):
        pieces.append(token if coefficient == 1 else f"{coefficient}*{token}")
    if constant or not pieces:
        pieces.append(hex(constant))
    return " + ".join(pieces)


class PcodeGraph:
    def __init__(self, rows: list[dict[str, str]]) -> None:
        self.rows = rows
        self.definitions: dict[str, dict[str, str]] = {}
        for row in rows:
            output = (row.get("output") or "").strip()
            if output in ("", "-"):
                continue
            if output in self.definitions:
                raise ValueError(f"duplicate P-code output token: {output}")
            self.definitions[output] = row

    def roots(self, token: str, seen: set[str] | None = None, depth: int = 0) -> set[str]:
        if seen is None:
            seen = set()
        if depth > 24 or token in seen:
            return {token}
        if constant_value(token) is not None:
            return set()
        row = self.definitions.get(token)
        if row is None:
            return {token}
        opcode = (row.get("opcode") or "").strip()
        inputs = split_inputs(row.get("inputs", ""))
        if not inputs:
            return {token}
        next_seen = set(seen)
        next_seen.add(token)
        if opcode == "LOAD":
            selected = (inputs[-1],)
        elif opcode in TRANSPARENT_OPS:
            selected = (inputs[0],)
        else:
            selected = inputs
        result: set[str] = set()
        for child in selected:
            result.update(self.roots(child, next_seen, depth + 1))
        return result

    def affine(
        self,
        token: str,
        seen: set[str] | None = None,
        depth: int = 0,
    ) -> tuple[dict[str, int], int] | None:
        if seen is None:
            seen = set()
        if depth > 24 or token in seen:
            return None
        constant = constant_value(token)
        if constant is not None:
            return {}, constant
        row = self.definitions.get(token)
        if row is None:
            return {token: 1}, 0
        opcode = (row.get("opcode") or "").strip()
        inputs = split_inputs(row.get("inputs", ""))
        next_seen = set(seen)
        next_seen.add(token)
        if opcode in TRANSPARENT_OPS and inputs:
            return self.affine(inputs[0], next_seen, depth + 1)
        if opcode in ("INT_ADD", "PTRSUB") and len(inputs) >= 2:
            return combine_affine(
                self.affine(inputs[0], next_seen, depth + 1),
                self.affine(inputs[1], next_seen, depth + 1),
                1,
            )
        if opcode == "INT_SUB" and len(inputs) >= 2:
            return combine_affine(
                self.affine(inputs[0], next_seen, depth + 1),
                self.affine(inputs[1], next_seen, depth + 1),
                -1,
            )
        if opcode == "PTRADD" and len(inputs) >= 3:
            base = self.affine(inputs[0], next_seen, depth + 1)
            index = self.affine(inputs[1], next_seen, depth + 1)
            scale = constant_value(inputs[2])
            if base is None or index is None or scale is None:
                return None
            scaled = (
                {key: value * scale for key, value in index[0].items()},
                index[1] * scale,
            )
            return combine_affine(base, scaled, 1)
        if opcode == "MULTIEQUAL" and inputs:
            values = [self.affine(child, next_seen, depth + 1) for child in inputs]
            if values and values[0] is not None and all(value == values[0] for value in values[1:]):
                return values[0]
        return None

    def expression(self, token: str, seen: set[str] | None = None, depth: int = 0) -> str:
        if seen is None:
            seen = set()
        if depth > 10:
            return token + "..."
        constant = constant_value(token)
        if constant is not None:
            return hex(constant)
        if token in seen:
            return token + "<cycle>"
        row = self.definitions.get(token)
        if row is None:
            return token
        opcode = (row.get("opcode") or "").strip()
        inputs = split_inputs(row.get("inputs", ""))
        next_seen = set(seen)
        next_seen.add(token)
        if opcode in TRANSPARENT_OPS and inputs:
            return self.expression(inputs[0], next_seen, depth + 1)
        if opcode == "CALL":
            name = (row.get("callee_name") or row.get("callee_entry") or "CALL").strip()
            return f"{name}(" + ", ".join(
                self.expression(child, next_seen, depth + 1) for child in inputs
            ) + ")"
        if opcode == "LOAD" and inputs:
            return f"LOAD[{self.expression(inputs[-1], next_seen, depth + 1)}]"
        if opcode in ("INT_ADD", "PTRSUB") and len(inputs) >= 2:
            return (
                f"({self.expression(inputs[0], next_seen, depth + 1)} + "
                f"{self.expression(inputs[1], next_seen, depth + 1)})"
            )
        if opcode == "INT_SUB" and len(inputs) >= 2:
            return (
                f"({self.expression(inputs[0], next_seen, depth + 1)} - "
                f"{self.expression(inputs[1], next_seen, depth + 1)})"
            )
        if opcode == "PTRADD" and len(inputs) >= 3:
            return (
                f"({self.expression(inputs[0], next_seen, depth + 1)} + "
                f"{self.expression(inputs[1], next_seen, depth + 1)}*"
                f"{self.expression(inputs[2], next_seen, depth + 1)})"
            )
        if opcode == "MULTIEQUAL":
            return "PHI(" + ", ".join(
                self.expression(child, next_seen, depth + 1) for child in inputs
            ) + ")"
        return opcode + "(" + ", ".join(
            self.expression(child, next_seen, depth + 1) for child in inputs
        ) + ")"


def parameter_index(source_id: str) -> int | None:
    match = PARAM_RE.match(source_id)
    return None if match is None else int(match.group(1), 10)


def load_identity(path: Path, entry: str) -> dict[str, str]:
    matches: list[dict[str, str]] = []
    for row in read_tsv(path):
        if (row.get("format") or "").strip() != IDENTITY_FORMAT:
            raise ValueError(f"{path}: expected {IDENTITY_FORMAT}")
        if entry in {
            (row.get("requested_entry") or "").strip(),
            (row.get("resolved_entry") or "").strip(),
        }:
            matches.append(row)
    if len(matches) != 1:
        raise ValueError(f"{path}: expected one identity for {entry}, got {len(matches)}")
    return matches[0]


def analyze(
    pcode_path: Path,
    subtree_path: Path,
    identities_path: Path,
) -> tuple[str, bool]:
    pcode_rows = read_tsv(pcode_path)
    if not pcode_rows:
        raise ValueError(f"{pcode_path}: empty P-code export")
    missing = sorted(PCODE_REQUIRED.difference(pcode_rows[0]))
    if missing:
        raise ValueError(f"{pcode_path}: missing columns: {', '.join(missing)}")
    function_entries = {(row.get("function_entry") or "").strip() for row in pcode_rows}
    if "" in function_entries or "-" in function_entries or len(function_entries) != 1:
        raise ValueError(f"{pcode_path}: expected exactly one function entry")
    function_entry = next(iter(function_entries))
    graph = PcodeGraph(pcode_rows)

    subtree_rows = read_tsv(subtree_path)
    relevant = [
        row
        for row in subtree_rows
        if (row.get("format") or "").strip() == SUBTREE_FORMAT
        and parameter_index((row.get("source_id") or "").strip()) is not None
    ]
    if not relevant:
        raise ValueError(f"{subtree_path}: no createWithFile parameter rows")
    factory_entries = {
        (row.get("source_entry") or "").strip()
        for row in relevant
        if (row.get("source_entry") or "").strip() not in ("", "-")
    }
    if len(factory_entries) != 1:
        raise ValueError(f"{subtree_path}: expected exactly one factory entry")
    factory_entry = next(iter(factory_entries))

    caller_roots = [
        row
        for row in relevant
        if (row.get("row_type") or "").strip() == "node"
        and (row.get("caller_entry") or "").strip() == function_entry
        and (row.get("depth") or "").strip() == "0"
    ]
    if not caller_roots:
        raise ValueError(f"{subtree_path}: no parameter roots for caller {function_entry}")
    call_sites = {(row.get("caller_call_site") or "").strip() for row in caller_roots}
    if "" in call_sites or "-" in call_sites or len(call_sites) != 1:
        raise ValueError(
            f"{subtree_path}: expected one createWithFile call site for {function_entry}, "
            f"got {sorted(call_sites)}"
        )
    call_site = next(iter(call_sites))
    parameters = sorted(
        {
            index
            for row in caller_roots
            if (index := parameter_index((row.get("source_id") or "").strip())) is not None
        }
    )
    if parameters != list(range(len(parameters))):
        raise ValueError(f"{subtree_path}: non-contiguous parameter indices: {parameters}")

    factory_calls = [
        row
        for row in pcode_rows
        if (row.get("opcode") or "").strip() == "CALL"
        and (row.get("op_address") or "").strip() == call_site
        and (row.get("callee_entry") or "").strip() == factory_entry
    ]
    if len(factory_calls) != 1:
        raise ValueError(
            f"{pcode_path}: expected one call to {factory_entry} at {call_site}, got {len(factory_calls)}"
        )
    factory_call = factory_calls[0]
    parameter_tokens = split_inputs(factory_call.get("inputs", ""))
    if len(parameter_tokens) != len(parameters):
        raise ValueError(
            f"{pcode_path}: factory call has {len(parameter_tokens)} inputs, "
            f"subtree evidence has {len(parameters)} parameters"
        )

    identity = load_identity(identities_path, function_entry)
    qualified_name = (identity.get("qualified_name") or "-").strip()
    signature = (identity.get("signature") or "-").strip()
    if "EnemyObject::initActionsSystem" not in qualified_name:
        raise ValueError(
            f"{identities_path}: caller identity mismatch for {function_entry}: {qualified_name}"
        )

    caller_nodes = [
        row
        for row in relevant
        if (row.get("row_type") or "").strip() == "node"
        and (row.get("caller_entry") or "").strip() == function_entry
        and (row.get("caller_call_site") or "").strip() == call_site
    ]
    nodes_by_parameter: dict[int, list[dict[str, str]]] = defaultdict(list)
    for row in caller_nodes:
        index = parameter_index((row.get("source_id") or "").strip())
        if index is not None:
            nodes_by_parameter[index].append(row)

    enemy_parameters: list[int] = []
    typed_enemy_roots: set[str] = set()
    for index, group in nodes_by_parameter.items():
        if any(
            "EnemyObject" in (
                (row.get("type_display") or "") + " " +
                (row.get("pointee_type_display") or "")
            )
            for row in group
        ):
            enemy_parameters.append(index)
        for row in group:
            type_text = (
                (row.get("type_display") or "") + " " +
                (row.get("pointee_type_display") or "")
            )
            token = (row.get("node_token") or "").strip()
            if "EnemyObject" in type_text and (
                token.startswith("register:") or token.startswith("input:")
            ):
                typed_enemy_roots.add(token)

    lines = [
        "EnemyObject initActionsSystem createWithFile call-site evidence",
        f"caller: {function_entry}:{qualified_name}",
        f"signature: {signature}",
        f"factory_entry: {factory_entry}",
        f"call_site: {call_site}",
        f"parameters: {len(parameter_tokens)}",
        "enemy_object_provenance_params: " + (
            ",".join(f"param[{index}]" for index in sorted(enemy_parameters)) or "-"
        ),
    ]

    load_details: dict[int, tuple[str, tuple[dict[str, int], int] | None]] = {}
    for index, token in enumerate(parameter_tokens):
        definition = graph.definitions.get(token)
        opcode = (definition.get("opcode") or "-").strip() if definition else "-"
        callee = (definition.get("callee_name") or "-").strip() if definition else "-"
        lines.append(
            f"param[{index}]: token={token} opcode={opcode} expr={graph.expression(token)}"
        )
        if callee not in ("", "-"):
            lines.append(f"  call_target: {callee}")
        if opcode == "LOAD" and definition is not None:
            inputs = split_inputs(definition.get("inputs", ""))
            address_token = inputs[-1] if inputs else "-"
            affine = graph.affine(address_token) if address_token != "-" else None
            load_details[index] = (address_token, affine)
            lines.append(f"  load_address_token: {address_token}")
            lines.append(f"  load_address: {format_affine(affine)}")
            if affine is not None and len(affine[0]) == 1:
                base_root, coefficient = next(iter(affine[0].items()))
                if coefficient == 1:
                    lines.append(f"  load_base_root: {base_root}")
                    lines.append(f"  load_offset: {hex(affine[1])}")
        if index in enemy_parameters:
            lines.append("  subtree_enemy_object_provenance: true")

    parameter_zero_definition = graph.definitions.get(parameter_tokens[0])
    parameter_zero_is_get_cstring = bool(
        parameter_zero_definition
        and (parameter_zero_definition.get("opcode") or "").strip() == "CALL"
        and "getCString" in (parameter_zero_definition.get("callee_name") or "")
    )
    receiver_token = "-"
    receiver_roots: set[str] = set()
    if parameter_zero_is_get_cstring and parameter_zero_definition is not None:
        inputs = split_inputs(parameter_zero_definition.get("inputs", ""))
        if inputs:
            receiver_token = inputs[0]
            receiver_roots = graph.roots(receiver_token)

    enemy_load_bases: list[str] = []
    for index in sorted(set(enemy_parameters).intersection(load_details)):
        affine = load_details[index][1]
        if affine is None or len(affine[0]) != 1:
            continue
        root, coefficient = next(iter(affine[0].items()))
        if coefficient == 1:
            enemy_load_bases.append(root)
    common_enemy_base = (
        enemy_load_bases[0]
        if enemy_load_bases and all(root == enemy_load_bases[0] for root in enemy_load_bases)
        else "-"
    )

    receiver_direct_input = (
        receiver_token != "-"
        and graph.definitions.get(receiver_token) is None
        and constant_value(receiver_token) is None
    )
    single_receiver_root = next(iter(receiver_roots)) if len(receiver_roots) == 1 else "-"
    signature_shape = (
        "EnemyObject * this" in signature
        and "CCString * param_1" in signature
    )
    receiver_consistent_with_ccstring_formal = (
        parameter_zero_is_get_cstring
        and receiver_direct_input
        and single_receiver_root not in ("", "-")
        and common_enemy_base not in ("", "-")
        and single_receiver_root != common_enemy_base
        and signature_shape
    )

    lines.append("structural conclusions:")
    lines.append(
        f"  param[0]_is_getCString_result: {'true' if parameter_zero_is_get_cstring else 'false'}"
    )
    lines.append(f"  getCString_receiver_token: {receiver_token}")
    lines.append(
        "  getCString_receiver_roots: " + (",".join(sorted(receiver_roots)) or "-")
    )
    lines.append(f"  common_enemy_object_load_base: {common_enemy_base}")
    lines.append(
        "  typed_enemy_object_roots: " + (",".join(sorted(typed_enemy_roots)) or "-")
    )
    lines.append(
        "  receiver_distinct_from_enemy_object_base: "
        + (
            "true"
            if single_receiver_root not in ("", "-")
            and common_enemy_base not in ("", "-")
            and single_receiver_root != common_enemy_base
            else "false"
        )
    )
    lines.append(
        "  signature_has_enemy_this_and_ccstring_param: "
        + ("true" if signature_shape else "false")
    )
    lines.append(
        "  getCString_receiver_consistent_with_sole_CCString_formal: "
        + ("true" if receiver_consistent_with_ccstring_formal else "false")
    )
    lines.append(
        "semantic status: structural call-site provenance only; "
        "filename/ID/object-type semantics require independent signature/mapping evidence"
    )

    complete = (
        parameter_zero_is_get_cstring
        and common_enemy_base != "-"
        and all(index in load_details for index in (1, 2, 7))
        and signature_shape
    )
    return "\n".join(lines) + "\n", complete


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_tsv", type=Path)
    parser.add_argument("subtree_tsv", type=Path)
    parser.add_argument("identities_tsv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    try:
        report, complete = analyze(args.pcode_tsv, args.subtree_tsv, args.identities_tsv)
    except (OSError, ValueError) as error:
        raise SystemExit(f"error: {error}")

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")
    return 0 if complete else 2


if __name__ == "__main__":
    raise SystemExit(main())
