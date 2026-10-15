#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parent
AUD=ROOT/'audit_nevergone_residual_closure_v24.py'
FLOW_FIELDS=['format','row_type','class_name','function_entry','function_name','owner_parameter_proven','owner_parameter_type','owner_parameter_name','op_address','op_time','call_kind','call_target_entry','call_target_name','arg_index','arg_token','arg_flow_kind','arg_owner_offset','address_token','address_flow_kind','address_owner_offset','value_token','store_count','call_count','owner_address_arg_count','owner_field_value_arg_count','owner_derived_unknown_arg_count','owner_derived_unknown_store_count']
WRITE_FIELDS=['format','class_name','function_entry','function_name','op_address','op_time','address_kind','address_base_token','address_offset','base_parameter_index','value_token','value_root_token','value_root_opcode','value_root_type_display','value_root_parameter_index','value_root_parameter_type_display','factory_target_entry','factory_target_name']
PCODE_FIELDS=['function_entry','op_address','op_time','opcode','output','inputs','callee_entry','callee_name']
SLICE_FIELDS=['format','row_type','function_entry','call_index','call_address','call_time','recovery_status','slot_offset','structural_receiver_token','node_id','parent_node_id','input_index','depth','node_token','type_display','pointer_depth','pointee_type_display','constant_value','defining_opcode','defining_address','defining_time','call_target_entry','call_target_name','call_target_return_type','call_target_is_thunk','call_target_thunk_target','truncation','node_count','truncated_by_budget','depth_truncation_count','cycle_count','max_nodes','max_depth']
def wt(path,fields,rows):
 with path.open('w',newline='',encoding='utf-8') as f:w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader();w.writerows(rows)
def flow_summary(cls,entry='1',owner='1',typ=None):
 r={k:'-' for k in FLOW_FIELDS};r.update(format='NEVERGONE.GhidraClassOwnerAliasFlow/1',row_type='summary',class_name=cls,function_entry=entry,function_name=cls+'::f',owner_parameter_proven=owner,owner_parameter_type=typ or cls+' *',owner_parameter_name='this',store_count='0',call_count='0',owner_address_arg_count='0',owner_field_value_arg_count='0',owner_derived_unknown_arg_count='0',owner_derived_unknown_store_count='0');return r
def write(cls,entry,off,callee,typ):
 r={k:'-' for k in WRITE_FIELDS};r.update(format='NEVERGONE.GhidraClassParameterFieldWrite/1',class_name=cls,function_entry=entry,function_name=cls+'::f',op_address='2',op_time='1',address_kind='base_plus_constant',address_base_token='this',address_offset=hex(off),base_parameter_index='0',value_token='v',value_root_token='def:CALL@2:1:4',value_root_opcode='CALL',value_root_type_display=typ,factory_target_entry='3',factory_target_name=callee);return r
def main():
 with tempfile.TemporaryDirectory() as td:
  d=Path(td);pdir=d/'p';sdir=d/'s';pdir.mkdir();sdir.mkdir()
  b=d/'b.tsv';j=d/'j.tsv';p=d/'p.tsv';w=d/'w.tsv';out=d/'out.json'
  wt(b,FLOW_FIELDS,[flow_summary('BeatenEffect')]);wt(j,FLOW_FIELDS,[flow_summary('CCJoyStick')]);wt(p,FLOW_FIELDS,[flow_summary('PVEDATA::equipToolsListMsg',typ='equipToolsListMsg *')])
  rows=[]
  for off,entry in [(8,'00299a66'),(12,'00299a88'),(16,'00299aaa'),(20,'00299acc')]:
   rows.append(write('PVEDATA::equipToolsListMsg',entry,off,'operator.new','equipToolsDataMsg *'))
   rows.append(write('PVEDATA::equipToolsListMsg','0029c20c',off,'PVEDATA::equipToolsDataMsg::default_instance','undefined4'))
  wt(w,WRITE_FIELDS,rows)
  for entry in ['00299a66','00299a88','00299aaa','00299acc']:
   wt(pdir/f'pcode-{entry}.tsv',PCODE_FIELDS,[
    {'function_entry':entry,'op_address':'10','op_time':'1','opcode':'CALL','output':'a','inputs':'const:0x10:4','callee_entry':'0070e330','callee_name':'operator.new'},
    {'function_entry':entry,'op_address':'12','op_time':'1','opcode':'CALL','output':'-','inputs':'a','callee_entry':'20','callee_name':'PVEDATA::equipToolsDataMsg::equipToolsDataMsg'},])
  sf={k:'-' for k in SLICE_FIELDS};sf.update(format='NEVERGONE.GhidraStructuralReceiverSlice/1',row_type='summary',function_entry='002a3e94',call_index='0',call_address='1',call_time='1',recovery_status='canonical_receiver',slot_offset='0x80',structural_receiver_token='x',node_count='1',truncated_by_budget='0',depth_truncation_count='0',cycle_count='0',max_nodes='10',max_depth='10')
  wt(sdir/'slice-002a3e94.tsv',SLICE_FIELDS,[sf])
  subprocess.run([sys.executable,str(AUD),'--flow','BeatenEffect',str(b),'--flow','CCJoyStick',str(j),'--flow','PVEDATA::equipToolsListMsg',str(p),'--proto-writes',str(w),'--pcode-dir',str(pdir),'--slice-dir',str(sdir),'--details-output',str(out)],check=True,capture_output=True,text=True)
  x=json.loads(out.read_text())
  assert x['namespace_owner_fix_observed'] is True
  assert all(v['mutable_operator_new_typed_nested'] for v in x['protobuf_field_writers'].values())
  assert all(v['default_instance_write_count']==1 for v in x['protobuf_field_writers'].values())
  assert all(v['nested_constructor_seen'] for v in x['protobuf_mutable_constructor_calls'].values())
  assert x['callind_slices']['slice-002a3e94'][0]['slot_offset']=='0x80'
 print('residual closure v24 smoke: ok')
if __name__=='__main__':main()
