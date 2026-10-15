#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FINAL=ROOT/'tools/ml/finalize_nevergone_residual_receiver_recovery_v19.py'
FMT='NEVERGONE.GhidraStructuralReceiverSlice/1'
H='format row_type function_entry call_index call_address call_time recovery_status slot_offset structural_receiver_token node_id parent_node_id input_index depth node_token type_display pointer_depth pointee_type_display constant_value defining_opcode defining_address defining_time call_target_entry call_target_name call_target_return_type call_target_is_thunk call_target_thunk_target truncation node_count truncated_by_budget depth_truncation_count cycle_count max_nodes max_depth'.split()

def jwrite(p,rows):
    p.parent.mkdir(parents=True,exist_ok=True)
    p.write_text(''.join(json.dumps(x,sort_keys=True)+'\n' for x in rows))
def twrite(p,rows,header=H):
    p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=header,delimiter='\t');w.writeheader();w.writerows(rows)
def inf(src,call,slot,status='unresolved_receiver_class',cls=None,vt=None,extra=None):
    r={'format':'NEVERGONE.StaticReceiverVtableInference/1','source_id':src,'call_index':call,'call_address':f'{call+1:08x}','dispatch_slot_offset':slot,'status':status,'class_name':cls,'receiver_vtable_id':vt,'candidate_receiver_class_names':[cls] if cls else [],'candidate_receiver_vtable_ids':[vt] if vt else [],'receiver_type_name':'int *','receiver_type_display':'int *','runtime_validation_status':'runtime_unobserved'}
    if extra:r.update(extra)
    return r
def q(r):
    return {'format':'NEVERGONE.StaticReceiverVtableQuery/1','source_id':r['source_id'],'call_index':r['call_index'],'call_address':r['call_address'],'receiver_vtable_id':r['receiver_vtable_id'],'dispatch_slot_offset':r['dispatch_slot_offset'],'receiver_class_name':r['class_name'],'receiver_class_match_kind':'anchor','receiver_class_evidence_source':'anchor','receiver_type_name':'int *','receiver_type_display':'int *','runtime_validation_status':'runtime_unobserved','evidence_ref':'anchor','affects_routing':False}
def base(entry,call,slot,token,count):
    d={x:'-' for x in H};d.update(format=FMT,row_type='summary',function_entry=entry,call_index=str(call),call_address=f'{call+1:08x}',call_time='1',recovery_status='canonical_receiver',slot_offset=slot,structural_receiver_token=token,node_count=str(count),truncated_by_budget='0',depth_truncation_count='0',cycle_count='0',max_nodes='160',max_depth='24');return d
def node(entry,call,slot,n,p,i,depth,token,op='-',const='-',target='-',name='-'):
    d={x:'-' for x in H};d.update(format=FMT,row_type='node',function_entry=entry,call_index=str(call),call_address=f'{call+1:08x}',call_time='1',recovery_status='canonical_receiver',slot_offset=slot,structural_receiver_token=token,node_id=str(n),parent_node_id=str(p),input_index=str(i),depth=str(depth),node_token=token,constant_value=const,defining_opcode=op,defining_address='00100000',defining_time='1',call_target_entry=target,call_target_name=name,max_nodes='160',max_depth='24');return d
def android(entry,call,slot,addr='shared-address'):
    return [base(entry,call,slot,'root',2),node(entry,call,slot,0,-1,-1,0,'root','LOAD'),node(entry,call,slot,1,0,1,1,addr,'CAST')]
def callroot(entry,call,slot,token='shared-call'):
    return [base(entry,call,slot,token,1),node(entry,call,slot,0,-1,-1,0,token,'CALL',target='00549614',name='cocos2d::CCSprite::create')]
def field(entry,call,slot,off):
    base_token='register:register:00000020:4'; rows=[base(entry,call,slot,'load',6),node(entry,call,slot,0,-1,-1,0,'load','LOAD'),node(entry,call,slot,1,0,1,1,'cast','CAST'),node(entry,call,slot,2,1,0,2,'ptr','PTRADD'),node(entry,call,slot,3,2,0,3,base_token),node(entry,call,slot,4,2,1,3,f'const:0x{off:x}:4','-',f'0x{off:x}'),node(entry,call,slot,5,2,2,3,'const:0x1:4','-','0x1')];return rows
def run(root,mutate=None):
    A='libcocos2dcpp.so:002a08fc';J='libcocos2dcpp.so:002a647e'
    rows=[inf(A,0,'0xd4'),inf(A,1,'0x8','resolved_unique_vtable','AndroidSdkMgr','libcocos2dcpp.so:008f3ce8',{'exact_singleton_cell_recovery':{'accepted':True,'singleton_cell':'0094cbd8'}}),inf(J,0,'0xec'),inf(J,1,'0x7c','resolved_unique_vtable','cocos2d::CCSprite','libcocos2dcpp.so:009363d8',{'call_root_factory_recovery':{'accepted':True,'callee_entry':'00549614','callee_name':'cocos2d::CCSprite::create'}}),inf(J,2,'0x54'),inf(J,3,'0xc0'),inf(J,4,'0x58','resolved_unique_vtable','cocos2d::CCSprite','libcocos2dcpp.so:009363d8',{'ccjoystick_field_recovery_v13':{'accepted':True,'owner_class':'CCJoyStick','field_offset':0x104}}),inf(J,5,'0x3c'),inf('libcocos2dcpp.so:002a47c6',6,'0x180'),inf('libcocos2dcpp.so:002a5dd0',0,'0x14c')]
    jwrite(root/'in.jsonl',rows);jwrite(root/'q.jsonl',[q(rows[1]),q(rows[3]),q(rows[6])])
    a=android('002a08fc',0,'0xd4')+android('002a08fc',1,'0x8','different' if mutate=='android' else 'shared-address');twrite(root/'s/a.tsv',a)
    jr=field('002a647e',0,'0xec',0x108)+callroot('002a647e',1,'0x7c')+callroot('002a647e',2,'0x54','different' if mutate=='call' else 'shared-call')+field('002a647e',3,'0xc0',0x108)+field('002a647e',4,'0x58',0x104)+field('002a647e',5,'0x3c',0x108);twrite(root/'s/j.tsv',jr)
    sh='format function_entry function_name op_address op_time address_kind address_base_token address_offset base_parameter_index value_token value_root_token value_root_opcode factory_target_entry factory_target_name'.split(); stores=[]
    for off in (0x104,0x108):stores.append({'format':'NEVERGONE.GhidraExactParameterFieldFactoryStore/1','function_entry':'002a6584','function_name':'CCJoyStick::init','op_address':'1','op_time':'1','address_kind':'base_plus_constant','address_base_token':'r0','address_offset':hex(off),'base_parameter_index':'0','value_token':'v','value_root_token':'c','value_root_opcode':'CALL','factory_target_entry':'deadbeef' if mutate=='factory' and off==0x108 else '005496e2','factory_target_name':'cocos2d::CCSprite::create'});twrite(root/'stores.tsv',stores,sh)
    cats=[]
    for cls,vt,slot,target in [('AndroidSdkMgr','libcocos2dcpp.so:008f3ce8','0xd4','a'),('cocos2d::CCSprite','libcocos2dcpp.so:009363d8','0xec','b'),('cocos2d::CCSprite','libcocos2dcpp.so:009363d8','0x54','c'),('cocos2d::CCSprite','libcocos2dcpp.so:009363d8','0xc0','d'),('cocos2d::CCSprite','libcocos2dcpp.so:009363d8','0x3c','e')]:cats.append({'format':'NEVERGONE.StaticVtableSlotCatalog/3','class_name':cls,'receiver_vtable_id':vt,'dispatch_slot_offset':slot,'static_status':'single_function_memory_pointer','candidate_target_ids':[target],'runtime_validation_status':'runtime_unobserved'});jwrite(root/'cat.jsonl',cats)
    cp=subprocess.run(['python3',str(FINAL),str(root/'in.jsonl'),str(root/'q.jsonl'),str(root/'s'),str(root/'stores.tsv'),str(root/'cat.jsonl'),'--output',str(root/'out.jsonl'),'--queries-output',str(root/'oq.jsonl'),'--details-output',str(root/'det.jsonl')],text=True,capture_output=True);assert cp.returncode==0,cp.stderr+cp.stdout
    return [json.loads(x) for x in (root/'det.jsonl').read_text().splitlines() if x]
def main():
    with tempfile.TemporaryDirectory() as d: assert sum(x['accepted'] for x in run(Path(d)/'ok'))==5
    for kind,want in [('android',4),('call',4),('factory',2)]:
        with tempfile.TemporaryDirectory() as d: assert sum(x['accepted'] for x in run(Path(d)/kind,kind))==want
    print('v19 residual receiver recovery smoke: pass')
if __name__=='__main__':main()
