#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location(
    "audit_v40", HERE / "audit_nevergone_beaten_script_engine_field_load_v40.py")
MOD = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MOD)

HEADER = (
    "format\trow_type\taccessor_entry\taccessor_name\tdirect_caller_functions\tcaller_functions_decompiled\t"
    "caller_functions_failed\taccessor_call_sites\tmanager_field_loads\tmanager_nonfield_uses\t"
    "loaded_value_receiver_calls\tloaded_value_nonreceiver_calls\tloaded_value_retention_stores\t"
    "loaded_value_return_escapes\tloaded_value_dereference_reads\tloaded_value_dereference_writes\t"
    "loaded_value_null_checks\tloaded_value_opaque_uses\tcaller_entry\tcaller_name\taccessor_call_site\t"
    "load_address\tload_offset\tuse_depth\tuse_opcode\tuse_address\tuse_input_index\tdirect_target_entry\t"
    "direct_target_name\tuse_kind\tstatus"
)


def fixture(root: Path, *, bad_offset: bool = False, escape: bool = False) -> tuple[Path, Path]:
    v39 = root / "v39.json"
    v39.write_text(json.dumps({
        "format": MOD.V39_FORMAT,
        "elf_sha256": MOD.SHA,
        "manager_class_nonnull_writer_frontier_closed": True,
        "engine_field_offset": "0x0",
        "shared_manager_return_use_export_complete": True,
        "shared_manager_accessor_call_sites": 33,
        "shared_manager_unsupported_uses": 33,
        "shared_manager_direct_member_uses": 0,
        "shared_manager_unsupported_opcode_counts": {"LOAD": 33},
        "receiver_promotions": 0,
        "affects_routing": False,
    }), encoding="utf-8")

    rows = [HEADER]
    for i in range(33):
        off = "0x4" if bad_offset and i == 32 else "0x0"
        site = f"005100{i:02x}"
        rows.append(
            f"{MOD.EXPORT_FORMAT}\tfield_load\t00546cdc\tcocos2d::CCScriptEngineManager::sharedManager\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t00520000\tcaller\t{site}\t00530000\t{off}\t1\tLOAD\t00530000\t1\t-\t-\tfield_load\tobserved"
        )
    rows.append(
        f"{MOD.EXPORT_FORMAT}\tloaded_use\t00546cdc\tcocos2d::CCScriptEngineManager::sharedManager\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t00520000\tcaller\t00510000\t00530000\t0x0\t2\tLOAD\t00530004\t1\t-\t-\tdereference_read\tobserved"
    )
    rows.append(
        f"{MOD.EXPORT_FORMAT}\tloaded_use\t00546cdc\tcocos2d::CCScriptEngineManager::sharedManager\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t00520000\tcaller\t00510000\t00530000\t0x0\t2\tCALLIND\t00530008\t1\t-\t-\treceiver_call\tobserved"
    )
    if escape:
        rows.append(
            f"{MOD.EXPORT_FORMAT}\tloaded_use\t00546cdc\tcocos2d::CCScriptEngineManager::sharedManager\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t00520000\tcaller\t00510000\t00530000\t0x0\t2\tSTORE\t0053000c\t2\t-\t-\tretention_store\tobserved"
        )
    retention = 1 if escape else 0
    rows.append(
        f"{MOD.EXPORT_FORMAT}\tsummary\t00546cdc\tcocos2d::CCScriptEngineManager::sharedManager\t30\t30\t0\t33\t33\t0\t1\t0\t{retention}\t0\t1\t0\t0\t0\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\tcomplete_field_load_export"
    )
    export = root / "v40.tsv"
    export.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return v39, export


def expect_fail(paths: tuple[Path, Path]) -> None:
    try:
        MOD.audit(*paths)
    except ValueError:
        return
    raise AssertionError("expected fail-closed rejection")


with tempfile.TemporaryDirectory() as temp:
    result = MOD.audit(*fixture(Path(temp)))
    assert result["shared_manager_engine_field_load_count"] == 33
    assert result["shared_manager_engine_field_load_frontier_closed"] is True
    assert result["loaded_engine_pointer_receiver_call_count"] == 1
    assert result["loaded_engine_pointer_escape_frontier_closed"] is True
    assert result["receiver_promotions"] == 0 and result["affects_routing"] is False

with tempfile.TemporaryDirectory() as temp:
    expect_fail(fixture(Path(temp), bad_offset=True))

with tempfile.TemporaryDirectory() as temp:
    result = MOD.audit(*fixture(Path(temp), escape=True))
    assert result["loaded_engine_pointer_retention_store_count"] == 1
    assert result["loaded_engine_pointer_escape_frontier_closed"] is False
    assert "loaded_engine_pointer_escape_or_opaque_use" in result["residual_script_engine_frontier"]

print("BeatenEffect script-engine field-load v40 smoke: PASS")
