#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FINAL=ROOT/'tools/ml/enforce_nevergone_receiver_parameter_root_safety_v20.py'

def wj(p,rows): p.write_text(''.join(json.dumps(x)+'\n' for x in rows),encoding='utf-8')
def wt(p,fields,rows):
    with p.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=fields,delimiter='\t'); w.writeheader(); w.writerows(rows)

def inf(s,i,src,cls,vt):
    return {'format':'NEVERGONE.StaticReceiverVtableInference/1','source_id':s,'call_index':i,'call_address':f'{0x1000+i:08x}','dispatch_slot_offset':'0x80','status':'resolved_unique_vtable','class_evidence_source':src,'class_name':cls,'class_match_kind':'x','candidate_receiver_class_names':[cls],'candidate_receiver_vtable_ids':[vt],'receiver_vtable_id':vt,'affects_routing':False}
def q(s,i,vt): return {'format':'NEVERGONE.StaticReceiverVtableQuery/1','source_id':s,'call_index':i,'call_address':f'{0x1000+i:08x}','dispatch_slot_offset':'0x80','receiver_vtable_id':vt,'receiver_class_name':'X','affects_routing':False}
def tr(s,i,param,slot='0x80',canonical=True,direct='0',match='-'):
    return {'format':'NEVERGONE.GhidraTargetReceiverProvenance/1','source_id':s,'call_index':str(i),'call_address':f'{0x1000+i:08x}','structural_status':'canonical_receiver' if canonical else 'vtable_not_load','target_slot_offset':slot,'parameter_index':str(param),'parameter_type_display':'A *','direct_receiver_present':direct,'direct_receiver_matches_structural':match}

def main():
  with tempfile.TemporaryDirectory() as td:
    t=Path(td); inp=t/'in.jsonl'; qs=t/'q.jsonl'; tg=t/'t.tsv'; out=t/'out.jsonl'; oq=t/'oq.jsonl'; det=t/'d.jsonl'
    rows=[inf('lib.so:1',0,'parameter_caller_type','A','vtA'),inf('lib.so:2',1,'parameter_caller_type','B','vtB'),inf('lib.so:3',2,'structural_parameter_caller_type','C','vtC'),{'format':'NEVERGONE.StaticReceiverVtableInference/1','source_id':'lib.so:4','call_index':3,'call_address':'00001003','dispatch_slot_offset':'0x80','status':'resolved_unique_vtable','class_evidence_source':'direct_receiver_type','class_name':'D','receiver_vtable_id':'vtD','affects_routing':False}]
    wj(inp,rows); wj(qs,[q('lib.so:1',0,'vtA'),q('lib.so:2',1,'vtB'),q('lib.so:3',2,'vtC'),q('lib.so:4',3,'vtD')])
    fields=['format','source_id','call_index','call_address','structural_status','target_slot_offset','parameter_index','parameter_type_display','direct_receiver_present','direct_receiver_matches_structural']
    wt(tg,fields,[tr('lib.so:1',0,0),tr('lib.so:2',1,-1),tr('lib.so:3',2,1,direct='1',match='1')])
    subprocess.run([sys.executable,str(FINAL),str(inp),str(qs),str(tg),'--output',str(out),'--queries-output',str(oq),'--details-output',str(det)],check=True)
    rr=[json.loads(x) for x in out.read_text().splitlines() if x.strip()]; by={x['source_id']:x for x in rr}
    assert by['lib.so:1']['status']=='resolved_unique_vtable'
    assert by['lib.so:2']['status']=='unresolved_receiver_class'
    assert by['lib.so:2']['blocked_class_name_v20']=='B'
    assert 'receiver_not_parameter_root' in by['lib.so:2']['receiver_parameter_root_safety_v20']['reasons']
    assert by['lib.so:3']['status']=='resolved_unique_vtable'
    assert by['lib.so:4']['status']=='resolved_unique_vtable'
    qq=[json.loads(x) for x in oq.read_text().splitlines() if x.strip()]
    assert {x['source_id'] for x in qq}=={'lib.so:1','lib.so:3','lib.so:4'}
    wt(tg,fields,[tr('lib.so:2',1,0)])
    subprocess.run([sys.executable,str(FINAL),str(inp),str(qs),str(tg),'--output',str(out),'--queries-output',str(oq)],check=True)
    by={x['source_id']:x for x in [json.loads(x) for x in out.read_text().splitlines() if x.strip()]}
    assert by['lib.so:1']['status']=='unresolved_receiver_class'
    assert 'missing_target_receiver_provenance' in by['lib.so:1']['receiver_parameter_root_safety_v20']['reasons']
  print('receiver parameter-root safety v20 smoke: ok')
  return 0
if __name__=='__main__': raise SystemExit(main())
