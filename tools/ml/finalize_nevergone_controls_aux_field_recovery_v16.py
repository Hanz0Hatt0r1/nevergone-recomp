#!/usr/bin/env python3
"""Resolve two CCJoyStick and one CCSprite slot-0x80 Controls-field receivers."""
from __future__ import annotations
import argparse,json
from pathlib import Path
from typing import Any

INF='NEVERGONE.StaticReceiverVtableInference/1';Q='NEVERGONE.StaticReceiverVtableQuery/1';ST='NEVERGONE.ControlsAuxFieldStability/1';CAT='NEVERGONE.StaticVtableSlotCatalog/3'
DETAIL='NEVERGONE.ControlsAuxFieldRecovery/1';FINAL='NEVERGONE.ControlsAuxFieldRecoveryFinalization/1';SLOT='0x80'
CALLS={
 ('libcocos2dcpp.so:002a74c0',3):('002a752a','0x124','CCJoyStick','libcocos2dcpp.so:008f4228','libcocos2dcpp.so:00525d2e'),
 ('libcocos2dcpp.so:002a7696',0):('002a76b6','0x124','CCJoyStick','libcocos2dcpp.so:008f4228','libcocos2dcpp.so:00525d2e'),
 ('libcocos2dcpp.so:002a7696',1):('002a76c4','0x174','cocos2d::CCSprite','libcocos2dcpp.so:009363d8','libcocos2dcpp.so:00549f1e'),
}
RES={'resolved_unique_vtable','resolved_unique_vtable_runtime_match','resolved_converged_target'}

def read(p:Path):return [json.loads(x) for x in p.read_text(encoding='utf-8').splitlines() if x.strip()]
def write(p:Path,rows):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(''.join(json.dumps(x,sort_keys=True)+'\n' for x in rows),encoding='utf-8')
def key(r,w):
 s=r.get('source_id');c=r.get('call_index')
 if not isinstance(s,str) or not isinstance(c,int):raise ValueError(w+': invalid identity')
 return s,c
def slot(v):
 try:return f"0x{int(str(v),0):x}"
 except:return None

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('v15_inference_jsonl',type=Path);p.add_argument('v15_queries_jsonl',type=Path);p.add_argument('aux_stability_jsonl',type=Path);p.add_argument('catalog_jsonl',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--queries-output',type=Path,required=True);p.add_argument('--details-output',type=Path);a=p.parse_args()
 inf=read(a.v15_inference_jsonl);qs=read(a.v15_queries_jsonl);st=read(a.aux_stability_jsonl);cat=read(a.catalog_jsonl)
 sums=[r for r in st if r.get('format')==ST and r.get('record_type')=='summary']
 if len(sums)!=1:raise ValueError('aux stability summary missing')
 s=sums[0]
 if not(s.get('scope')=='exact_packaged_apk_normal_code_execution' and s.get('target_fields')==['0x124','0x174'] and s.get('controls_aux_fields_stable') is True and s.get('external_full_this_static_frontier_closed') is True and s.get('whole_program_alias_writes_excluded') is False and s.get('affects_routing') is False):raise ValueError('aux stability summary rejected')
 by={}
 for r in st:
  if r.get('format')==ST and r.get('record_type')=='receiver_field':
   k=key(r,'stability');
   if k in by:raise ValueError('duplicate stability row')
   by[k]=r
 if set(by)!=set(CALLS):raise ValueError('aux stability call set changed')
 cat_by={}
 for k,(_addr,_field,cls,vt,target) in CALLS.items():
  ck=(cls,vt)
  if ck in cat_by:continue
  rows=[r for r in cat if r.get('format')==CAT and r.get('class_name')==cls and r.get('receiver_vtable_id')==vt and slot(r.get('dispatch_slot_offset'))==SLOT]
  if len(rows)!=1:raise ValueError('catalog row not unique: '+str(ck))
  r=rows[0]
  if not(r.get('static_status')=='single_function_memory_pointer' and r.get('candidate_target_ids')==[target] and r.get('runtime_validation_status')!='runtime_mismatch'):raise ValueError('catalog target changed: '+str(ck))
  cat_by[ck]=r
 qmap={}
 for n,r in enumerate(qs,1):
  if r.get('format')!=Q:raise ValueError(f'query:{n}: wrong format')
  k=key(r,f'query:{n}')
  if k in qmap:raise ValueError('duplicate query')
  qmap[k]=dict(r)
 if set(CALLS)&set(qmap):raise ValueError('v15 already queries v16 target')
 out=[];det=[];seen=set();accepted=0
 for n,orig in enumerate(inf,1):
  if orig.get('format')!=INF:raise ValueError(f'inference:{n}: wrong format')
  r=dict(orig);k=key(r,f'inference:{n}')
  if k not in CALLS:out.append(r);continue
  seen.add(k);addr,field,cls,vt,target=CALLS[k];proof=by[k];reasons=[]
  if r.get('status')!='unresolved_receiver_class':reasons.append('v15_row_not_unresolved_receiver_class')
  if r.get('call_address')!=addr:reasons.append('call_address_mismatch')
  if slot(r.get('dispatch_slot_offset'))!=SLOT:reasons.append('slot_mismatch')
  if not(proof.get('call_address')==addr and proof.get('receiver_field_offset')==field and proof.get('receiver_class_name')==cls and proof.get('receiver_vtable_id')==vt and proof.get('slot_target_id')==target and proof.get('packaged_field_stability_proven') is True and proof.get('external_full_this_static_frontier_closed') is True and proof.get('affects_routing') is False):reasons.append('aux_stability_receiver_mismatch')
  rec={'format':DETAIL,'source_id':k[0],'call_index':k[1],'call_address':addr,'dispatch_slot_offset':SLOT,'receiver_field_offset':field,'accepted':False,'receiver_class_name':cls,'receiver_vtable_id':vt,'candidate_target_ids':[target],'packaged_field_stability_proven':True,'external_full_this_static_frontier_closed':True,'whole_program_alias_writes_excluded':False,'reasons':reasons,'affects_routing':False}
  if not reasons:
   rec['accepted']=True;accepted+=1;r['class_name']=cls;r['class_match_kind']='controls_aux_field_packaged_stability';r['class_evidence_source']='slot80_receiver_field_factory_plus_controls_aux_packaged_closure';r['candidate_receiver_class_names']=[cls];r['candidate_receiver_vtable_ids']=[vt];r['receiver_vtable_id']=vt;r['status']='resolved_unique_vtable';r['controls_aux_field_recovery_v16']=rec
   cr=cat_by[(cls,vt)];qmap[k]={'format':Q,'source_id':k[0],'call_index':k[1],'call_address':addr,'receiver_vtable_id':vt,'dispatch_slot_offset':SLOT,'receiver_class_name':cls,'receiver_class_match_kind':r['class_match_kind'],'receiver_class_evidence_source':r['class_evidence_source'],'receiver_type_name':r.get('receiver_type_name'),'receiver_type_display':r.get('receiver_type_display'),'runtime_validation_status':cr.get('runtime_validation_status','runtime_unobserved'),'evidence_ref':f'{k[0]}#call={k[1]}','affects_routing':False}
  else:r['controls_aux_field_recovery_v16']=rec
  det.append(rec);out.append(r)
 if seen!=set(CALLS):raise ValueError('v15 missing v16 rows')
 if accepted!=3:raise ValueError(f'expected 3 accepted, got {accepted}')
 outq=[qmap[k] for k in sorted(qmap)];resolved={key(r,'out') for r in out if r.get('status') in RES}
 if resolved!=set(qmap):raise ValueError('v16 resolved/query identity mismatch')
 write(a.output,out);write(a.queries_output,outq)
 if a.details_output:write(a.details_output,det)
 print('format:',FINAL);print('aux field recoveries accepted:',accepted);print('query delta: +3');print('whole-program alias exclusion: no');print('output:',a.output);return 0
if __name__=='__main__':raise SystemExit(main())
