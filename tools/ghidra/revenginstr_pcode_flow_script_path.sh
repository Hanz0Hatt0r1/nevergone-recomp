#!/usr/bin/env bash

revenginstr_pcode_flow_script_path() {
  local repo_root=$1
  local shared_repo=${REVENGINSTR_REPO:-$repo_root/../revenginstr}
  local local_scripts="$repo_root/tools/ghidra"
  local shared_exporter="$shared_repo/ghidra/ExportFunctionPcodeFlow.java"

  if [[ -f "$shared_exporter" ]]; then
    printf '%s;%s' "$shared_repo/ghidra" "$local_scripts"
  else
    printf '%s' "$local_scripts"
  fi
}
