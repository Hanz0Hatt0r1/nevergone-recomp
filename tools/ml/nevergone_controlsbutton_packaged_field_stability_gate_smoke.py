#!/usr/bin/env python3
"""Synthetic fail-closed checks for the packaged field stability join gate."""
from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
GATE=ROOT/"tools/ml/gate_nevergone_controlsbutton_packaged_field_stability.py"


def write(path:Path,rows:list[dict])->None:
    path.write_text("".join(json.dumps(r,sort_keys=True)+"\n" for r in rows),encoding="utf-8")


def fixture():
    prelim=[{
        "format":"NEVERGONE.ControlsButtonPackagedFieldStability/1",
        "record_type":"summary","scope":"exact_packaged_apk_normal_code_execution",
        "packaged_controlsbutton_field_stability_proven":True,
        "whole_program_alias_writes_excluded":False,
        "external_injected_code_excluded":False,"affects_routing":False,
    }]
    for call in (3,4,7):
        prelim.append({
            "format":"NEVERGONE.ControlsButtonPackagedFieldStability/1",
            "record_type":"receiver_field","source_id":"libcocos2dcpp.so:002a7594",
            "call_index":call,"affects_routing":False,
        })
    full=[{
        "format":"NEVERGONE.ControlsFullThisStaticClosure/1",
        "external_full_this_sites":14,
        "direct_sites":[{"site":str(i)} for i in range(11)],
        "indirect_sites":[{"site":str(i)} for i in range(3)],
        "external_full_this_static_frontier_closed":True,
        "known_frontier_sites_write_controls_pointer_fields":False,
        "whole_program_alias_writes_excluded":False,
        "receiver_class_proven":False,"affects_routing":False,
    }]
    return prelim,full


def run(prelim,full,ok:bool):
    with tempfile.TemporaryDirectory() as td:
        d=Path(td); p=d/"p.jsonl"; f=d/"f.jsonl"; out=d/"out.jsonl"
        write(p,prelim);write(f,full)
        r=subprocess.run([sys.executable,str(GATE),str(p),str(f),"--output",str(out)],
                         capture_output=True,text=True)
        if ok:
            assert r.returncode==0,(r.stdout,r.stderr)
            rows=[json.loads(x) for x in out.read_text().splitlines() if x.strip()]
            assert rows[0]["external_full_this_static_frontier_closed"] is True
            assert rows[0]["external_full_this_sites_closed"]==14
            assert rows[0]["known_external_frontier_writes_receiver_fields"] is False
            assert all(row.get("external_full_this_static_frontier_closed") is True
                       for row in rows[1:])
        else:
            assert r.returncode!=0,(r.stdout,r.stderr)


def main():
    prelim,full=fixture();run(prelim,full,True)
    bad=copy.deepcopy(full);bad[0]["external_full_this_static_frontier_closed"]=False;run(prelim,bad,False)
    bad=copy.deepcopy(full);bad[0]["known_frontier_sites_write_controls_pointer_fields"]=True;run(prelim,bad,False)
    bad=copy.deepcopy(full);bad[0]["direct_sites"].pop();run(prelim,bad,False)
    bad=copy.deepcopy(prelim);bad[0]["packaged_controlsbutton_field_stability_proven"]=False;run(bad,full,False)
    print("ControlsButton packaged field stability gate smoke: ok")

if __name__=="__main__":main()
