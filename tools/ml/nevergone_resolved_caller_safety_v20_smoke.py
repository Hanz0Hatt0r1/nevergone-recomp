#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
AUDIT=ROOT/'tools/ml/audit_nevergone_resolved_caller_safety.py'

def wj(p,rows): p.write_text(''.join(json.dumps(x)+'\n' for x in rows),encoding='utf-8')
def wt(p,fields,rows):
    with p.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=fields,delimiter='\t'); w.writeheader(); w.writerows(rows)

def inf(s,i,src,cls): return {'format':'NEVERGONE.StaticReceiverVtableInference/1','source_id':s,'call_index':i,'call_address':f'{0x2000+i:08x}','dispatch_slot_offset':'0x80','status':'resolved_unique_vtable','class_evidence_source':src,'class_name':cls}
def comp(s,i,cls): return {'format':'NEVERGONE.GhidraResolvedCallerTypeCompleteness/1','source_id':s,'call_index':str(i),'status':'complete_typed_callers','direct_caller_functions':'1','caller_functions_decompiled':'1','caller_functions_failed':'0','matched_caller_calls':'1','typed_caller_calls':'1','untyped_caller_calls':'0','caller_single_pointer_pointee_displays_json':json.dumps([cls]),'caller_single_pointer_pointee_names_json':'[]'}
def tar(s,i,param,slot='0x80',direct='0',match='-'): return {'format':'NEVERGONE.GhidraTargetReceiverProvenance/1','source_id':s,'call_index':str(i),'call_address':f'{0x2000+i:08x}','structural_status':'canonical_receiver','target_slot_offset':slot,'parameter_index':str(param),'parameter_type_display':'X *','direct_receiver_present':direct,'direct_receiver_matches_structural':match}
def cat(cls): return {'format':'NEVERGONE.StaticVtableSlotCatalog/3','class_name':cls}

def main():
  with tempfile.TemporaryDirectory() as td:
    t=Path(td); I=t/'i.jsonl'; C=t/'c.tsv'; T=t/'t.tsv'; K=t/'k.jsonl'; D=t/'d.jsonl'
    wj(I,[inf('lib.so:1',0,'parameter_caller_type','A'),inf('lib.so:2',1,'parameter_caller_type','B'),inf('lib.so:3',2,'structural_parameter_caller_type','C')])
    cf=list(comp('lib.so:1',0,'A').keys()); wt(C,cf,[comp('lib.so:1',0,'A'),comp('lib.so:2',1,'B'),comp('lib.so:3',2,'C')])
    tf=list(tar('lib.so:1',0,0).keys()); wt(T,tf,[tar('lib.so:1',0,0),tar('lib.so:2',1,-1),tar('lib.so:3',2,1,slot='0x84')])
    wj(K,[cat('A'),cat('B'),cat('C')])
    p=subprocess.run([sys.executable,str(AUDIT),str(I),str(C),str(T),str(K),'--details-output',str(D)],text=True,capture_output=True)
    assert p.returncode==3,p.stdout+p.stderr
    rows=[json.loads(x) for x in D.read_text().splitlines() if x.strip()]; by={(x['source_id'],x['call_index']):x for x in rows}
    assert by[('lib.so:1',0)]['status']=='safe_caller_recovery'
    assert by[('lib.so:1',0)]['receiver_roots_at_parameter'] is True
    assert by[('lib.so:2',1)]['status']=='unsafe_caller_recovery'
    assert 'receiver_not_parameter_root' in by[('lib.so:2',1)]['reasons']
    assert by[('lib.so:3',2)]['status']=='unsafe_caller_recovery'
    assert 'target_receiver_slot_mismatch' in by[('lib.so:3',2)]['reasons']
    wj(I,[inf('lib.so:3',2,'structural_parameter_caller_type','C')]); wt(C,cf,[comp('lib.so:3',2,'C')]); wt(T,tf,[tar('lib.so:3',2,1,direct='1',match='1')])
    p=subprocess.run([sys.executable,str(AUDIT),str(I),str(C),str(T),str(K)],text=True,capture_output=True)
    assert p.returncode==0,p.stdout+p.stderr
  print('resolved caller safety v20 smoke: ok'); return 0
if __name__=='__main__': raise SystemExit(main())
