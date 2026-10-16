#!/usr/bin/env bash

revenginstr_ghidra_script_path() {
  local repo_root=$1
  local shared_repo=${REVENGINSTR_REPO:-$repo_root/../revenginstr}
  local local_scripts="$repo_root/tools/ghidra"
  shift

  local exporter
  for exporter in "$@"; do
    [[ -f "$shared_repo/ghidra/$exporter" ]] || { printf '%s' "$local_scripts"; return; }
  done
  printf '%s;%s' "$shared_repo/ghidra" "$local_scripts"
}

revenginstr_pcode_flow_script_path() {
  revenginstr_ghidra_script_path "$1" ExportFunctionPcodeFlow.java
}

revenginstr_bounded_callgraph_script_path() {
  revenginstr_ghidra_script_path "$1" \
    ExportBoundedCallGraphPcodeFlow.java \
    ExportBoundedReverseCallGraphPcodeFlow.java
}

revenginstr_address_reference_script_path() {
  revenginstr_ghidra_script_path "$1" ExportGhidraAddressReferenceOwners.java
}
