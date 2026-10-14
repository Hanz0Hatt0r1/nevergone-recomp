#!/usr/bin/env python3
"""Join preliminary ControlsButton field stability with the exact 14-site full-this closure."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

STABILITY_FORMAT="NEVERGONE.ControlsButtonPackagedFieldStability/1"
FULL_FORMAT="NEVERGONE.ControlsFullThisStaticClosure/1"


def read(path:Path)->list[dict]:
    rows=[json.loads(x) for x in path.read_text(encoding="utf-8").splitlines() if x.strip()]
    if not rows or not all(isinstance(x,dict) for x in rows): raise ValueError("invalid JSONL: "+str(path))
    return rows


def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("preliminary_stability_jsonl",type=Path)
    p.add_argument("full_this_static_closure_jsonl",type=Path)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    rows=read(a.preliminary_stability_jsonl); full=read(a.full_this_static_closure_jsonl)
    summaries=[r for r in rows if r.get("format")==STABILITY_FORMAT and r.get("record_type")=="summary"]
    if len(summaries)!=1: raise ValueError("expected one preliminary stability summary")
    s=summaries[0]
    if not (s.get("packaged_controlsbutton_field_stability_proven") is True and
            s.get("scope")=="exact_packaged_apk_normal_code_execution" and
            s.get("whole_program_alias_writes_excluded") is False and
            s.get("external_injected_code_excluded") is False and
            s.get("affects_routing") is False):
        raise ValueError("preliminary stability contract not accepted")
    if len(full)!=1: raise ValueError("expected one full-this closure row")
    f=full[0]
    if not (f.get("format")==FULL_FORMAT and f.get("external_full_this_sites")==14 and
            f.get("external_full_this_static_frontier_closed") is True and
            f.get("known_frontier_sites_write_controls_pointer_fields") is False and
            f.get("whole_program_alias_writes_excluded") is False and
            f.get("receiver_class_proven") is False and f.get("affects_routing") is False):
        raise ValueError("14-site packaged full-this closure not accepted")
    direct=f.get("direct_sites"); indirect=f.get("indirect_sites")
    if not isinstance(direct,list) or len(direct)!=11 or not isinstance(indirect,list) or len(indirect)!=3:
        raise ValueError("14-site frontier row count mismatch")
    out=[]
    for row in rows:
        item=dict(row)
        if item.get("record_type")=="summary":
            item["external_full_this_static_frontier_closed"]=True
            item["external_full_this_sites_closed"]=14
            item["known_external_frontier_writes_receiver_fields"]=False
        elif item.get("record_type")=="receiver_field":
            item["external_full_this_static_frontier_closed"]=True
        out.append(item)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text("".join(json.dumps(r,sort_keys=True)+"\n" for r in out),encoding="utf-8")
    print("format:",STABILITY_FORMAT)
    print("full-this packaged frontier: 14/14 closed")
    print("known frontier writes receiver fields: no")
    print("whole-program alias exclusion: no")
    print("output:",a.output)
    return 0

if __name__=="__main__": raise SystemExit(main())
