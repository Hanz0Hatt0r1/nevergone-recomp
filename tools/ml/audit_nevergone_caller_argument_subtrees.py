#!/usr/bin/env python3
from __future__ import annotations

import argparse, csv, json
from collections import Counter, defaultdict
from pathlib import Path

SLICE_FMT = "NEVERGONE.GhidraCallerArgumentSubtree/1"
CAT_FMT = "NEVERGONE.StaticVtableSlotCatalog/3"
AUDIT_FMT = "NEVERGONE.CallerArgumentSubtreeAudit/1"


def read_jsonl(path: Path):
    with path.open(encoding="utf-8") as f:
        return [json.loads(x) for x in f if x.strip()]


def clean_type(s: str | None):
    if not s or s == "-": return None
    s = s.strip()
    for prefix in ("class ", "struct "):
        if s.startswith(prefix): s = s[len(prefix):]
    return s.strip()


def map_class(name: str | None, catalog: set[str], terminals: dict[str, set[str]]) -> set[str]:
    n = clean_type(name)
    if not n: return set()
    if n in catalog: return {n}
    hits = terminals.get(n.rsplit("::", 1)[-1], set())
    return set(hits) if len(hits) == 1 else set()


def dec(v, default=-1):
    try: return int(v, 10)
    except Exception: return default


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("subtree_tsv", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("--details-output", type=Path)
    a = p.parse_args()

    catalog_rows = read_jsonl(a.catalog_jsonl)
    catalog = set()
    for i, r in enumerate(catalog_rows, 1):
        if r.get("format") != CAT_FMT: raise ValueError(f"{a.catalog_jsonl}:{i}: expected {CAT_FMT}")
        if isinstance(r.get("class_name"), str): catalog.add(r["class_name"])
    terminals = defaultdict(set)
    for c in catalog: terminals[c.rsplit("::", 1)[-1]].add(c)

    with a.subtree_tsv.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    grouped = defaultdict(list)
    for i, r in enumerate(rows, 2):
        if r.get("format") != SLICE_FMT: raise ValueError(f"{a.subtree_tsv}:{i}: expected {SLICE_FMT}")
        grouped[(r.get("source_id"), dec(r.get("source_call_index")))].append(r)

    details = []
    buckets = Counter()
    root_ops = Counter()
    terminal_ops = Counter()

    for key in sorted(grouped):
        rs = grouped[key]
        summary = next((r for r in rs if r.get("row_type") == "summary"), None)
        nodes = [r for r in rs if r.get("row_type") == "node"]
        by_call = defaultdict(list)
        for r in nodes:
            by_call[(r.get("caller_entry"), r.get("caller_call_site"))].append(r)

        call_details = []
        converged = []
        for call_key, ns in sorted(by_call.items()):
            by_id = {dec(r.get("node_id")): r for r in ns}
            children = defaultdict(list)
            for r in ns:
                pid = dec(r.get("parent_node_id"))
                if pid >= 0: children[pid].append(dec(r.get("node_id")))
            root = by_id.get(0)
            if root: root_ops[root.get("defining_opcode") or "-"] += 1

            value_equiv = {0}
            changed = True
            while changed:
                changed = False
                for nid, r in by_id.items():
                    if nid in value_equiv: continue
                    pid = dec(r.get("parent_node_id"))
                    if pid in value_equiv and r.get("edge_kind") == "value":
                        value_equiv.add(nid); changed = True

            classes = set()
            typed_nodes = []
            for nid in sorted(value_equiv):
                r = by_id.get(nid)
                if not r or dec(r.get("pointer_depth"), 0) != 1: continue
                mapped = map_class(r.get("pointee_type_display"), catalog, terminals)
                if mapped:
                    classes.update(mapped)
                    typed_nodes.append({"node_id": nid, "type": r.get("type_display"), "classes": sorted(mapped)})

            leaves = []
            for nid, r in sorted(by_id.items()):
                if children.get(nid): continue
                op = r.get("defining_opcode") or "-"
                terminal_ops[op] += 1
                leaves.append({
                    "node_id": nid, "edge": r.get("edge_kind"), "opcode": op,
                    "token": r.get("node_token"), "type": r.get("type_display"),
                    "call_target": r.get("call_target_name"), "truncation": r.get("truncation"),
                })
            call_details.append({
                "caller_entry": call_key[0], "call_site": call_key[1],
                "root_opcode": root.get("defining_opcode") if root else None,
                "value_classes": sorted(classes), "value_typed_nodes": typed_nodes, "leaves": leaves,
            })
            converged.append(classes)

        if summary is None:
            status = "missing_summary"
        elif summary.get("status") != "complete_caller_argument_subtrees":
            status = "caller_coverage_incomplete"
        elif not call_details:
            status = "no_matched_caller_calls"
        elif all(len(c) == 1 for c in converged) and len(set(next(iter(c)) for c in converged)) == 1:
            status = "all_caller_arguments_converge_one_value_class"
        elif any(c for c in converged):
            status = "caller_argument_value_types_inconclusive"
        else:
            status = "caller_argument_requires_nontransparent_provenance"
        buckets[status] += 1
        chosen = sorted(set().union(*converged)) if converged else []
        detail = {
            "format": AUDIT_FMT, "source_id": key[0], "call_index": key[1],
            "summary_status": summary.get("status") if summary else None,
            "direct_caller_functions": dec(summary.get("direct_caller_functions")) if summary else -1,
            "matched_caller_calls": dec(summary.get("matched_caller_calls")) if summary else -1,
            "candidate_value_classes": chosen, "calls": call_details, "status": status, "affects_routing": False,
        }
        details.append(detail)

    print("== caller argument subtree audit ==")
    print("format:", AUDIT_FMT)
    print("target rows:", len(details))
    print("bucket counts:", dict(buckets))
    print("root opcode counts:", dict(root_ops))
    print("terminal opcode counts:", dict(terminal_ops))
    print("\nrows:")
    for d in details:
        print(d["source_id"], "call", d["call_index"], "callers=", d["direct_caller_functions"],
              "matched=", d["matched_caller_calls"], "classes=", d["candidate_value_classes"], "status=", d["status"])
        for c in d["calls"]:
            print("  caller", c["caller_entry"], "site", c["call_site"], "root=", c["root_opcode"],
                  "value_classes=", c["value_classes"])
            for leaf in c["leaves"]:
                print("    leaf", leaf)
    if a.details_output:
        a.details_output.parent.mkdir(parents=True, exist_ok=True)
        with a.details_output.open("w", encoding="utf-8") as f:
            for d in details: f.write(json.dumps(d, sort_keys=True) + "\n")
        print("details output:", a.details_output)
    print("affects routing: no")
    return 0

if __name__ == "__main__": raise SystemExit(main())
