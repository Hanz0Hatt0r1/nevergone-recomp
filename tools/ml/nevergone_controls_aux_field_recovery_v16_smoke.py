#!/usr/bin/env python3
from __future__ import annotations
import copy,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];FINAL=ROOT/'tools/ml/finalize_nevergone_controls_aux_field_recovery_v16.py'
CALLS={('libcocos2dcpp.so:002a74c0',3):('002a752a','0x124','CCJoyStick','libcocos2dcpp.so:008f4228','libcocos2dcpp.so:00525d2e'),('libcocos2dcpp.so:002a7696',0):('002a76b6','0x124','CCJoyStick','libcocos2dcpp.so:008f4228','libcocos2dcpp.so:00525d2e'),('libcocos2dcpp.so:002a7696',1):('002a76c4','0x174','cocos2d::CCSprite','libcocos2dcpp.so:009363d8','libcocos2dcpp.so:00549f1e')}
def w(p,r):p.write_text(''.join(json.dumps(x,sort_keys=True)+'\n' for x in r),encoding='utf-8')
def fixture():
 inf=[];st=[{'format':'NEVERGONE.ControlsAuxFieldStability/1','record_type':'summary','scope':'exact_packaged_apk_normal_code_execution','target_fields':['0x124','0x174'],'controls_aux_fields_stable':True,'external_full_this_static_frontier_closed':True,'whole_program_alias_writes_excluded':False,'affects_routing':False}]
 for (s,c),(a,f,cl,v,t) in CALLS.items():
  inf.append({'format':'NEVERGONE.StaticReceiverVtableInference/1','source_id':s,'call_index':c,'call_address':a,'dispatch_slot_offset':'0x80','status':'unresolved_receiver_class'})
  st.append({'format':'NEVERGONE.ControlsAuxFieldStability/1','record_type':'receiver_field','source_id':s,'call_index':c,'call_address':a,'receiver_field_offset':f,'receiver_class_name':cl,'receiver_vtable_id':v,'slot_target_id':t,'packaged_field_stability_proven':True,'external_full_this_static_frontier_closed':True,'affects_routing':False})
 cat=[{'format':'NEVERGONE.StaticVtableSlotCatalog/3','class_name':'CCJoyStick','receiver_vtable_id':'libcocos2dcpp.so:008f4228','dispatch_slot_offset':'0x80','static_status':'single_function_memory_pointer','candidate_target_ids':['libcocos2dcpp.so:00525d2e'],'runtime_validation_status':'runtime_unobserved'},{'format':'NEVERGONE.StaticVtableSlotCatalog/3','class_name':'cocos2d::CCSprite','receiver_vtable_id':'libcocos2dcpp.so:009363d8','dispatch_slot_offset':'0x80','static_status':'single_function_memory_pointer','candidate_target_ids':['libcocos2dcpp.so:00549f1e'],'runtime_validation_status':'runtime_unobserved'}]
 return inf,[],st,cat
def run(inf,q,st,cat,ok):
 with tempfile.TemporaryDirectory() as td:
  d=Path(td);ps=[d/x for x in 'iqsc'];out=d/'o';oq=d/'oq';det=d/'d'
  for p,r in zip(ps,(inf,q,st,cat)):w(p,r)
  x=subprocess.run([sys.executable,str(FINAL),*[str(p) for p in ps],'--output',str(out),'--queries-output',str(oq),'--details-output',str(det)],capture_output=True,text=True)
  if ok:
   assert x.returncode==0,(x.stdout,x.stderr);rows=[json.loads(z) for z in out.read_text().splitlines() if z.strip()];assert len(rows)==3 and all(r['status']=='resolved_unique_vtable' for r in rows)
  else:assert x.returncode!=0,(x.stdout,x.stderr)
def main():
 inf,q,st,cat=fixture();run(inf,q,st,cat,True)
 b=copy.deepcopy(st);b[0]['external_full_this_static_frontier_closed']=False;run(inf,q,b,cat,False)
 b=copy.deepcopy(st);b[1]['receiver_field_offset']='0x12c';run(inf,q,b,cat,False)
 b=copy.deepcopy(inf);b[0]['status']='resolved_unique_vtable';run(b,q,st,cat,False)
 b=copy.deepcopy(cat);b[0]['candidate_target_ids']=['dead'];run(inf,q,st,b,False)
 print('Controls aux field recovery v16 smoke: ok')
if __name__=='__main__':main()
