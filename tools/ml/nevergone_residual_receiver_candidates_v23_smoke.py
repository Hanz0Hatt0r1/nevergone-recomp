#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];AUD=ROOT/'tools/ml/audit_nevergone_residual_receiver_candidates_v23.py'
WF=['format','class_name','function_entry','function_name','op_address','op_time','address_kind','address_base_token','address_offset','base_parameter_index','value_token','value_root_token','value_root_opcode','value_root_type_display','value_root_parameter_index','value_root_parameter_type_display','factory_target_entry','factory_target_name']
SF=['format','row_type','function_entry','call_index','call_address','call_time','recovery_status','slot_offset','structural_receiver_token','node_id','parent_node_id','input_index','depth','node_token','type_display','pointer_depth','pointee_type_display','constant_value','defining_opcode','defining_address','defining_time','call_target_entry','call_target_name','call_target_return_type','call_target_is_thunk','call_target_thunk_target','truncation','node_count','truncated_by_budget','depth_truncation_count','cycle_count','max_nodes','max_depth']
def wt(p,fields,rows):
 with p.open('w',newline='',encoding='utf-8') as f:w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader();w.writerows(rows)
def wr(cls,off,factory):
 return {'format':'NEVERGONE.GhidraClassParameterFieldWrite/1','class_name':cls,'function_entry':'1','function_name':cls+'::init','op_address':'2','op_time':'1','address_kind':'base_plus_constant','address_base_token':'this','address_offset':hex(off),'base_parameter_index':'0','value_token':'v','value_root_token':'def:CALL@2:1:4','value_root_opcode':'CALL','value_root_type_display':factory+' *','value_root_parameter_index':'-','value_root_parameter_type_display':'-','factory_target_entry':'3','factory_target_name':factory}
def sl(entry,items):
 rows=[]
 for idx,slot,off in items:
  b={'format':'NEVERGONE.GhidraStructuralReceiverSlice/1','function_entry':entry,'call_index':str(idx),'call_address':'1','call_time':'1','recovery_status':'canonical_receiver','slot_offset':slot,'structural_receiver_token':'x','node_id':'-','parent_node_id':'-1','input_index':'-1','depth':'0','node_token':'-','type_display':'-','pointer_depth':'-','pointee_type_display':'-','constant_value':'-','defining_opcode':'-','defining_address':'-','defining_time':'-1','call_target_entry':'-','call_target_name':'-','call_target_return_type':'-','call_target_is_thunk':'-','call_target_thunk_target':'-','truncation':'-','node_count':'2','truncated_by_budget':'0','depth_truncation_count':'0','cycle_count':'0','max_nodes':'10','max_depth':'10'}
  rows.append({**b,'row_type':'summary'});rows.append({**b,'row_type':'node','node_id':'0','constant_value':hex(off),'defining_opcode':'LOAD'})
 return rows

def main():
 with tempfile.TemporaryDirectory() as td:
  d=Path(td);bw=d/'bw';jw=d/'jw';pw=d/'pw';bs=d/'bs';js=d/'js';ps=d/'ps';fa=d/'fa';out=d/'out'
  beat=[wr('BeatenEffect',o,'cocos2d::CCSprite::create') for o in [0x128,0x12c,0x130,0x134,0x138]]
  joy=[wr('CCJoyStick',0x10c,'cocos2d::CCSprite::create')]
  proto=[wr('PVEDATA::equipToolsListMsg',o,'PVEDATA::equipToolsDataMsg') for o in [8,12,16,20]]
  wt(bw,WF,beat);wt(jw,WF,joy);wt(pw,WF,proto)
  wt(bs,SF,sl('002a47c6',[(0,'0x180',0x130),(1,'0x180',0x134),(2,'0x180',0x138),(3,'0x180',0x12c),(4,'0x180',0x128),(5,'0x180',0x158)]))
  wt(js,SF,sl('002a651c',[(0,'0xec',0x10c),(1,'0xb8',0x10c)]))
  wt(ps,SF,sl('00299d90',[(0,'0x4',8),(1,'0x4',12),(2,'0x4',16),(3,'0x4',20)]))
  audits=[]
  for cls in ['BeatenEffect','CCJoyStick','PVEDATA::equipToolsListMsg']:
   audits.append({'format':'NEVERGONE.OwnerAliasFlowAudit/1','class_name':cls,'pointer_slot_alias_stability_proven':True})
  fa.write_text(''.join(json.dumps(x)+'\n' for x in audits))
  subprocess.run([sys.executable,str(AUD),str(bw),str(jw),str(pw),str(bs),str(js),str(ps),str(fa),'--details-output',str(out)],check=True)
  rows=[json.loads(x) for x in out.read_text().splitlines() if x.strip()]
  proven=[x for x in rows if x['receiver_class_proven']]
  assert len(proven)==11,(len(rows),len(proven),rows)
  b5=[x for x in rows if x['source_id'].endswith('002a47c6') and x['call_index']==5][0]
  assert not b5['receiver_class_proven'] and 'no_nonnull_factory_write' in b5['reasons']
 print('residual receiver candidates v23 smoke: ok')
if __name__=='__main__':main()
