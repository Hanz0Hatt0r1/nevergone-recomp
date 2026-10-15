#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
AUD=ROOT/'tools/ml/audit_nevergone_residual_owner_alias_frontier_v22.py'
WF=['format','class_name','function_entry','function_name','op_address','op_time','address_kind','address_base_token','address_offset','base_parameter_index','value_token','value_root_token','value_root_opcode','value_root_type_display','value_root_parameter_index','value_root_parameter_type_display','factory_target_entry','factory_target_name']
AF=['format','row_type','class_name','function_entry','function_name','op_address','op_time','decompile_status','address_kind','address_root_token','address_offset','address_parameter_index','address_contains_param0','value_token','call_kind','call_target_entry','call_target_name','param0_arg_indices','param0_arg_offsets','param0_unknown_arg_indices','store_count','call_count','param0_unknown_store_count','param0_call_count','param0_unknown_call_arg_count']
SF=['format','row_type','function_entry','call_index','call_address','call_time','recovery_status','slot_offset','structural_receiver_token','node_id','parent_node_id','input_index','depth','node_token','type_display','pointer_depth','pointee_type_display','constant_value','defining_opcode','defining_address','defining_time','call_target_entry','call_target_name','call_target_return_type','call_target_is_thunk','call_target_thunk_target','truncation','node_count','truncated_by_budget','depth_truncation_count','cycle_count','max_nodes','max_depth']
def wt(path,fields,rows):
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader();w.writerows(rows)
def wr(cls,off,root='def:CALL@1:1:4',factory='cocos2d::CCSprite::create'):
    return {'format':'NEVERGONE.GhidraClassParameterFieldWrite/1','class_name':cls,'function_entry':'00001000','function_name':cls+'::init','op_address':'00001010','op_time':'1','address_kind':'base_plus_constant','address_base_token':'register:r0:4','address_offset':off,'base_parameter_index':'0','value_token':root,'value_root_token':root,'value_root_opcode':'CALL' if factory else '-','value_root_type_display':'undefined4','value_root_parameter_index':'-','value_root_parameter_type_display':'-','factory_target_entry':'005496e2' if factory else '-','factory_target_name':factory or '-'}
def summary(cls,unknown_store='0'):
    return {'format':'NEVERGONE.GhidraClassOwnerAliasInventory/1','row_type':'summary','class_name':cls,'function_entry':'00001000','function_name':cls+'::init','op_address':'-','op_time':'-','decompile_status':'ok','address_kind':'-','address_root_token':'-','address_offset':'-','address_parameter_index':'-','address_contains_param0':'-','value_token':'-','call_kind':'-','call_target_entry':'-','call_target_name':'-','param0_arg_indices':'-','param0_arg_offsets':'-','param0_unknown_arg_indices':'-','store_count':'1','call_count':'0','param0_unknown_store_count':unknown_store,'param0_call_count':'0','param0_unknown_call_arg_count':'0'}
def slice_rows():
    rows=[]
    for idx,off in enumerate([0x130,0x134,0x138,0x12c,0x128,0x158]):
        rows.append({'format':'NEVERGONE.GhidraStructuralReceiverSlice/1','row_type':'summary','function_entry':'002a47c6','call_index':str(idx),'call_address':f'{0x2a47d8+idx*2:08x}','call_time':'1','recovery_status':'canonical_receiver','slot_offset':'0x180','structural_receiver_token':'x','node_id':'-','parent_node_id':'-','input_index':'-','depth':'-','node_token':'-','type_display':'-','pointer_depth':'-','pointee_type_display':'-','constant_value':'-','defining_opcode':'-','defining_address':'-','defining_time':'-1','call_target_entry':'-','call_target_name':'-','call_target_return_type':'-','call_target_is_thunk':'-','call_target_thunk_target':'-','truncation':'-','node_count':'2','truncated_by_budget':'0','depth_truncation_count':'0','cycle_count':'0','max_nodes':'10','max_depth':'10'})
        rows.append({**rows[-1],'row_type':'node','node_id':'0','parent_node_id':'-1','input_index':'-1','depth':'0','constant_value':hex(off),'defining_opcode':'LOAD'})
    rows.append({**rows[0],'row_type':'summary','call_index':'6','call_address':'002a4838','cycle_count':'1'})
    rows.append({**rows[1],'call_index':'6','call_address':'002a4838','constant_value':'0x14c','defining_opcode':'INT_ADD'})
    rows.append({**rows[1],'call_index':'6','call_address':'002a4838','node_id':'1','constant_value':'0x4','defining_opcode':'INT_ADD'})
    return rows
def run(tmp,unsafe=False):
    bw=tmp/'bw.tsv';jw=tmp/'jw.tsv';ba=tmp/'ba.tsv';ja=tmp/'ja.tsv';bs=tmp/'bs.tsv';out=tmp/'out.jsonl'
    beat=[wr('BeatenEffect',hex(x)) for x in [0x128,0x12c,0x130,0x134,0x138]]
    beat += [wr('BeatenEffect',hex(x),'const:0x0:4',None) for x in [0x14c,0x150,0x154,0x158]]
    joy=[wr('CCJoyStick','0x14c','const:0x0:4',None)]
    wt(bw,WF,beat);wt(jw,WF,joy)
    brow=[summary('BeatenEffect','1' if unsafe else '0')]; jrow=[summary('CCJoyStick')]
    wt(ba,AF,brow);wt(ja,AF,jrow);wt(bs,SF,slice_rows())
    cp=subprocess.run([sys.executable,str(AUD),str(bw),str(jw),str(ba),str(ja),str(bs),'--details-output',str(out)],check=True,capture_output=True,text=True)
    rows=[json.loads(x) for x in out.read_text().splitlines()]
    return rows,cp.stdout
def main():
    with tempfile.TemporaryDirectory() as td:
        rows,_=run(Path(td),False)
        by={r['call_index']:r for r in rows if r['source_id'].endswith('002a47c6')}
        assert by[0]['receiver_class_proven'] is True and by[4]['candidate_class_name']=='cocos2d::CCSprite'
        assert by[5]['receiver_class_proven'] is False and 'no_nonnull_factory_write' in by[5]['reasons']
        assert by[6]['receiver_field_offsets']==['0x14c','0x150','0x154'] and by[6]['receiver_class_proven'] is False
        rows,_=run(Path(td),True)
        assert not any(r['receiver_class_proven'] for r in rows if r['source_id'].endswith('002a47c6'))
    print('residual owner alias frontier v22 smoke: ok')
if __name__=='__main__':main()
