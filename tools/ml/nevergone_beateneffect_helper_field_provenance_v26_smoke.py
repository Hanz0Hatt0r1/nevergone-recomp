#!/usr/bin/env python3
import csv,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
AUD=ROOT/'tools/ml/audit_nevergone_beateneffect_helper_field_provenance_v26.py'
SF=['format','function_entry','function_name','op_address','op_time','address_kind','address_base_token','address_offset','base_parameter_index','value_token','value_root_token','value_root_opcode','factory_target_entry','factory_target_name']
CF=['format','row_type','source_id','source_call_index','source_entry','parameter_index','dispatch_slot_offset','expected_direct_caller_functions','expected_matched_caller_calls','direct_caller_functions','caller_functions_decompiled','caller_functions_failed','callers_with_matched_calls','matched_caller_calls','caller_entry','caller_name','caller_call_site','node_id','parent_node_id','input_index','depth','edge_kind','node_token','type_display','pointer_depth','pointee_type_display','constant_value','defining_opcode','defining_address','defining_time','call_target_entry','call_target_name','truncation','status']
FMT='NEVERGONE.GhidraExactParameterFieldFactoryStore/1';CFMT='NEVERGONE.GhidraCallerArgumentSubtree/1'
def wt(p,fields,rows):
 with p.open('w',encoding='utf-8',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader();w.writerows(rows)
def store(fn,param,off=0,factory='cocos2d::CCSprite::createWithSpriteFrame'):
 return {'format':FMT,'function_entry':fn,'function_name':'x','op_address':'1','op_time':'0','address_kind':'base_plus_constant','address_base_token':'input:x:4','address_offset':hex(off),'base_parameter_index':str(param),'value_token':'def:CALL@1:1:4','value_root_token':'def:CALL@1:1:4','value_root_opcode':'CALL','factory_target_entry':'0054968c','factory_target_name':factory}
def summary(fn,direct,matched):
 return {'format':CFMT,'row_type':'summary','source_id':'x','source_call_index':'0','source_entry':fn,'parameter_index':'1','dispatch_slot_offset':'-','expected_direct_caller_functions':'0','expected_matched_caller_calls':'0','direct_caller_functions':str(direct),'caller_functions_decompiled':str(direct),'caller_functions_failed':'0','callers_with_matched_calls':str(direct),'matched_caller_calls':str(matched),'caller_entry':'-','caller_name':'-','caller_call_site':'-','node_id':'-','parent_node_id':'-','input_index':'-','depth':'-','edge_kind':'-','node_token':'-','type_display':'-','pointer_depth':'-','pointee_type_display':'-','constant_value':'-','defining_opcode':'-','defining_address':'-','defining_time':'-','call_target_entry':'-','call_target_name':'-','truncation':'-','status':'caller_coverage_changed'}
def nodes(fn,site,off):
 base={'format':CFMT,'row_type':'node','source_id':'x','source_call_index':'0','source_entry':fn,'parameter_index':'1','dispatch_slot_offset':'-','expected_direct_caller_functions':'0','expected_matched_caller_calls':'0','direct_caller_functions':'-','caller_functions_decompiled':'-','caller_functions_failed':'-','callers_with_matched_calls':'-','matched_caller_calls':'-','caller_entry':'00aa0000','caller_name':'Caller','caller_call_site':site,'truncation':'-','status':'-','call_target_entry':'-','call_target_name':'-'}
 r0=dict(base,node_id='0',parent_node_id='-1',input_index='-1',depth='0',edge_kind='root',node_token='def:INT_ADD@1:1:4',type_display='CCSprite **',pointer_depth='2',pointee_type_display='CCSprite *',constant_value='-',defining_opcode='INT_ADD',defining_address='1',defining_time='1')
 r1=dict(base,node_id='1',parent_node_id='0',input_index='0',depth='1',edge_kind='base',node_token='register:r0:4',type_display='BeatenEffect *',pointer_depth='1',pointee_type_display='BeatenEffect',constant_value='-',defining_opcode='-',defining_address='-',defining_time='-')
 r2=dict(base,node_id='2',parent_node_id='0',input_index='1',depth='1',edge_kind='offset',node_token=f'const:{hex(off)}:4',type_display='int',pointer_depth='0',pointee_type_display='-',constant_value=hex(off),defining_opcode='-',defining_address='-',defining_time='-')
 return [r0,r1,r2]
def main():
 with tempfile.TemporaryDirectory() as td:
  t=Path(td); h1=t/'h1.tsv';h2=t/'h2.tsv';call=t/'call.tsv';cat=t/'cat.jsonl';out=t/'out.json'
  wt(h1,SF,[store('002a3a20',1)]);wt(h2,SF,[store('002a3c74',2)])
  cr=[summary('002a3a20',1,1),*nodes('002a3a20','0010',0x158),summary('002a3c74',3,3)]
  for i,o in enumerate([0x14c,0x150,0x154]):cr+=nodes('002a3c74',f'002{i}',o)
  wt(call,CF,cr)
  cs=[]
  for e in ['00526124','005353f8','00535204','00525dac','0051f3c8']:
   p=t/f'{e}.tsv';wt(p,SF,[store(e,0,0x20,factory='-')]);cs += ['--owner-callee-store',e,str(p)]
  cat.write_text(json.dumps({'format':'NEVERGONE.StaticVtableSlotCatalog/3','class_name':'BeatenEffect','dispatch_slot_offset':'0xbc','candidate_target_ids':['libcocos2dcpp.so:00525dac']})+'\n')
  cmd=['python3',str(AUD),'--helper-store','002a3a20',str(h1),'--helper-store','002a3c74',str(h2),'--callers',str(call),*cs,'--catalog',str(cat),'--details-output',str(out)]
  subprocess.run(cmd,check=True,capture_output=True,text=True);d=json.loads(out.read_text())
  assert d['helpers']['002a3a20']['field_factory_proven'] and d['helpers']['002a3c74']['field_factory_proven']
  assert set(d['proven_factory_fields'])=={'0x14c','0x150','0x154','0x158'}
  assert d['all_known_whole_owner_callees_target_field_clean'] and d['slot_0xbc_addchild_exact']
  cr=[summary('002a3a20',1,1),*nodes('002a3a20','0010',0x160),summary('002a3c74',3,3)]
  for i,o in enumerate([0x14c,0x150,0x154]):cr+=nodes('002a3c74',f'002{i}',o)
  wt(call,CF,cr);subprocess.run(cmd,check=True,capture_output=True,text=True);d=json.loads(out.read_text())
  assert not d['helpers']['002a3a20']['field_factory_proven']
 print('v26 helper field provenance smoke: pass')
if __name__=='__main__':main()
