#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];AUD=ROOT/'tools/ml/audit_nevergone_newly_unresolved_field_shapes_v22.py'
SF=['format','row_type','function_entry','call_index','call_address','call_time','recovery_status','slot_offset','structural_receiver_token','node_id','parent_node_id','input_index','depth','node_token','type_display','pointer_depth','pointee_type_display','constant_value','defining_opcode','defining_address','defining_time','call_target_entry','call_target_name','call_target_return_type','call_target_is_thunk','call_target_thunk_target','truncation','node_count','truncated_by_budget','depth_truncation_count','cycle_count','max_nodes','max_depth']
CF=['format','address','block','block_start','block_end','read','write','execute','primary_symbol','function_at_entry','function_at_name','function_at_body','function_at_is_thunk','function_at_thunk_target','function_containing_entry','function_containing_name','function_containing_body','instruction_mnemonic','data_type','raw_u32']
def wt(p,fs,rows):
 with p.open('w',newline='',encoding='utf-8') as f:w=csv.DictWriter(f,fieldnames=fs,delimiter='\t');w.writeheader();w.writerows(rows)
def slices(entry,count,offset0):
 out=[]
 for i in range(count):
  base={'format':'NEVERGONE.GhidraStructuralReceiverSlice/1','function_entry':entry,'call_index':str(i),'call_address':f'{int(entry,16)+0x10+i*4:08x}','call_time':'1','recovery_status':'canonical_receiver','slot_offset':'0x4','structural_receiver_token':'x','node_id':'-','parent_node_id':'-','input_index':'-','depth':'-','node_token':'-','type_display':'-','pointer_depth':'-','pointee_type_display':'-','constant_value':'-','defining_opcode':'-','defining_address':'-','defining_time':'-1','call_target_entry':'-','call_target_name':'-','call_target_return_type':'-','call_target_is_thunk':'-','call_target_thunk_target':'-','truncation':'-','node_count':'2','truncated_by_budget':'0','depth_truncation_count':'0','cycle_count':'0','max_nodes':'20','max_depth':'10'}
  out.append({**base,'row_type':'summary'});out.append({**base,'row_type':'node','node_id':'0','parent_node_id':'-1','input_index':'-1','depth':'0','constant_value':hex(offset0+i*4),'defining_opcode':'LOAD'})
 return out
def ident(addr,name):
 return {'format':'NEVERGONE.GhidraAddressClassification/1','address':addr,'block':'.text','block_start':'0','block_end':'ffffffff','read':'1','write':'0','execute':'1','primary_symbol':'-','function_at_entry':addr,'function_at_name':name,'function_at_body':addr+'..'+addr,'function_at_is_thunk':'0','function_at_thunk_target':'-','function_containing_entry':addr,'function_containing_name':name,'function_containing_body':addr+'..'+addr,'instruction_mnemonic':'push','data_type':'-','raw_u32':'0x0'}
def main():
 with tempfile.TemporaryDirectory() as td:
  t=Path(td);a=t/'a.tsv';b=t/'b.tsv';ids=t/'ids.tsv';out=t/'out.jsonl'
  wt(a,SF,slices('00299d90',4,0x120));wt(b,SF,slices('002a651c',2,0x104));wt(ids,CF,[ident('00299d90','Foo::bar'),ident('002a651c','CCJoyStick::baz')])
  subprocess.run([sys.executable,str(AUD),str(a),str(b),str(ids),'--details-output',str(out)],check=True)
  rows=[json.loads(x) for x in out.read_text().splitlines()]
  assert len(rows)==6
  assert rows[0]['function_name']=='Foo::bar' and rows[0]['plausible_field_offsets']==['0x120']
  assert rows[-1]['function_name']=='CCJoyStick::baz' and rows[-1]['plausible_field_offsets']==['0x108']
 print('newly unresolved field shapes v22 smoke: ok')
if __name__=='__main__':main()
