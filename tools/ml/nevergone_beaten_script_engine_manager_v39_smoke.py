#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import json
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location(
    "audit_v39", HERE / "audit_nevergone_beaten_script_engine_manager_v39.py")
MOD = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MOD)


def write(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8")


def fixture(
    root: Path,
    *,
    unsafe: bool = False,
    unsupported: bool = True,
    injected: bool = False,
) -> tuple[Path, Path, Path, Path]:
    v38 = root / "v38.json"
    v38.write_text(json.dumps({
        "format": "NEVERGONE.BeatenScriptEngineV38/1",
        "elf_sha256": MOD.SHA,
        "packaged_dynamic_lookup_of_set_script_engine_excluded": True,
        "external_or_injected_runtime_code_excluded": injected,
        "whole_program_alias_writes_to_manager_engine_slot_excluded": False,
        "script_side_effects_closed": False,
        "receiver_promotions": 0,
    }), encoding="utf-8")

    field = root / "field.tsv"
    rows = [
        "format\tclass_name\tfunction_entry\tfunction_name\top_address\top_time\taddress_kind\taddress_base_token\taddress_offset\tbase_parameter_index\tvalue_token\tvalue_root_token\tvalue_root_opcode\tvalue_root_type_display\tvalue_root_parameter_index\tvalue_root_parameter_type_display\tfactory_target_entry\tfactory_target_name",
        "NEVERGONE.GhidraClassParameterFieldWrite/1\tcocos2d::CCScriptEngineManager\t00536ccc\tcocos2d::CCScriptEngineManager::setScriptEngine\t00536cd0\t0\tbase_plus_constant\tregister:r0:4\t0x0\t0\tregister:r1:4\tregister:r1:4\t-\tCCScriptEngineProtocol *\t1\tCCScriptEngineProtocol *\t-\t-",
        "NEVERGONE.GhidraClassParameterFieldWrite/1\tcocos2d::CCScriptEngineManager\t00536cc0\tcocos2d::CCScriptEngineManager::~CCScriptEngineManager\t00536cc4\t0\tbase_plus_constant\tregister:r0:4\t0x0\t0\tconst:0x0:4\tconst:0x0:4\t-\tuint\t-\t-\t-\t-",
    ]
    if unsafe:
        rows.append(
            "NEVERGONE.GhidraClassParameterFieldWrite/1\tcocos2d::CCScriptEngineManager\t00536cb0\tcocos2d::CCScriptEngineManager::mystery\t00536cb2\t0\tbase_plus_constant\tregister:r0:4\t0x0\t0\tregister:r2:4\tregister:r2:4\t-\tvoid *\t2\tvoid *\t-\t-")
    write(field, "\n".join(rows) + "\n")

    flow = root / "flow.tsv"
    header = "format\trow_type\tclass_name\tfunction_entry\tfunction_name\towner_parameter_proven\towner_parameter_type\towner_parameter_name\top_address\top_time\tcall_kind\tcall_target_entry\tcall_target_name\targ_index\targ_token\targ_flow_kind\targ_owner_offset\taddress_token\taddress_flow_kind\taddress_owner_offset\tvalue_token\tstore_count\tcall_count\towner_address_arg_count\towner_field_value_arg_count\towner_derived_unknown_arg_count\towner_derived_unknown_store_count"
    summary_setter = "NEVERGONE.GhidraClassOwnerAliasFlow/1\tsummary\tcocos2d::CCScriptEngineManager\t00536ccc\tcocos2d::CCScriptEngineManager::setScriptEngine\t1\tCCScriptEngineManager *\tthis\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t1\t0\t0\t0\t0\t0"
    summary_dtor = "NEVERGONE.GhidraClassOwnerAliasFlow/1\tsummary\tcocos2d::CCScriptEngineManager\t00536cc0\tcocos2d::CCScriptEngineManager::~CCScriptEngineManager\t1\tCCScriptEngineManager *\tthis\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t1\t0\t0\t0\t0\t0"
    write(flow, header + "\n" + summary_setter + "\n" + summary_dtor + "\n")

    consumers = root / "consumers.tsv"
    cheader = "format\trow_type\taccessor_entry\taccessor_name\tdirect_caller_functions\tcaller_functions_decompiled\tcaller_functions_failed\taccessor_call_sites\taccessor_calls_with_output\tdirect_member_uses\tunsupported_uses\tcaller_entry\tcaller_name\taccessor_call_site\tuse_depth\tuse_opcode\tuse_address\tuse_input_index\tdirect_target_entry\tdirect_target_name\tstatus"
    if unsupported:
        use = "NEVERGONE.GhidraAccessorReturnConsumers/1\tuse\t00536cdc\tcocos2d::CCScriptEngineManager::sharedManager\t-\t-\t-\t-\t-\t-\t-\t00516a00\tcocos2d::CCNode::CCNode\t00516a20\t0\tLOAD\t00516a24\t1\t-\t-\tnontransparent_use"
    else:
        use = "NEVERGONE.GhidraAccessorReturnConsumers/1\tuse\t00536cdc\tcocos2d::CCScriptEngineManager::sharedManager\t-\t-\t-\t-\t-\t-\t-\t00516a00\tcocos2d::CCNode::CCNode\t00516a20\t0\tCALL\t00516a24\t1\t00537000\tcocos2d::CCScriptEngineManager::safeMethod\tdirect_this_call"
    summary = (
        "NEVERGONE.GhidraAccessorReturnConsumers/1\tsummary\t00536cdc\tcocos2d::CCScriptEngineManager::sharedManager\t1\t1\t0\t1\t1\t"
        + ("0\t1" if unsupported else "1\t0")
        + "\t-\t-\t-\t-\t-\t-\t-\t-\t-\tcomplete_return_use_export"
    )
    write(consumers, cheader + "\n" + use + "\n" + summary + "\n")
    return v38, field, flow, consumers


def expect_fail(paths: tuple[Path, Path, Path, Path]) -> None:
    try:
        MOD.audit(*paths)
    except ValueError:
        return
    raise AssertionError("expected fail-closed rejection")


with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    result = MOD.audit(*fixture(root))
    assert result["engine_field_offset"] == "0x0"
    assert result["manager_engine_field_safe_zero_write_count"] == 1
    assert result["manager_engine_field_unsafe_nonsetter_write_count"] == 0
    assert result["manager_class_nonnull_writer_frontier_closed"] is True
    assert result["shared_manager_unsupported_uses"] == 1
    assert result["shared_manager_direct_accessor_escape_frontier_closed"] is False
    assert result["singleton_backing_cell_direct_access_bounded"] is False
    assert result["receiver_promotions"] == 0 and result["affects_routing"] is False

with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    result = MOD.audit(*fixture(root, unsafe=True))
    assert result["manager_engine_field_unsafe_nonsetter_write_count"] == 1
    assert result["manager_class_nonnull_writer_frontier_closed"] is False

with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    expect_fail(fixture(root, injected=True))

print("BeatenEffect script-engine manager v39 smoke: PASS")
