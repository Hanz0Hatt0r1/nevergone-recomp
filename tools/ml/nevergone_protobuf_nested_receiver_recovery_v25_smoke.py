#!/usr/bin/env python3
import csv,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FMT_INF='NEVERGONE.StaticReceiverVtableInference/1';FMT_Q='NEVERGONE.StaticReceiverVtableQuery/1';FMT_SL='NEVERGONE.GhidraStructuralReceiverSlice/1';FMT_W='NEVERGONE.GhidraClassParameterFieldWrite/1';FMT_F='NEVERGONE.GhidraClassOwnerAliasFlow/1';FMT_C='NEVERGONE.StaticVtableSlotCatalog/3'
FIELDS=[0x8,0xc,0x10,0x14];MUT=['00299a66','00299a88','00299aaa','00299acc']
def jwrite(p,rows):p.write_text(''.join(json.dumps(x)+'\n' for x in rows))
def twrite(p,rows):
 keys=list(rows[0]);
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=keys,delimiter='\t');w.writeheader();w.writerows(rows)
def main():
 final=ROOT/'tools/ml/finalize_nevergone_protobuf_nested_receiver_recovery_v25.py'
 with tempfile.TemporaryDirectory() as td:
  t=Path(td);inf=t/'i';q=t/'q';sl=t/'s';wr=t/'w';flow=t/'f';cat=t/'c';pc=t/'pc';pc.mkdir()
  jwrite(inf,[{'format':FMT_INF,'source_id':'libcocos2dcpp.so:00299d90','call_index':i,'call_address':f'00299d{0xa8+i*10:x}','dispatch_slot_offset':'0x4','status':'unresolved_receiver_class','class_name':None,'receiver_vtable_id':None,'receiver_type_name':'-','receiver_type_display':'-'} for i in range(4)])
  q.write_text('')
  sr=[]
  for i,field in enumerate(FIELDS):
   base={'format':FMT_SL,'function_entry':'00299d90','call_index':str(i),'call_address':f'00299d{0xa8+i*10:x}','row_type':'summary','recovery_status':'canonical_receiver','slot_offset':'0x4','truncated_by_budget':'0','depth_truncation_count':'0','cycle_count':'0','constant_value':'-'};sr.append(base)
   n=dict(base);n.update(row_type='node',constant_value=f'0x{field:x}');sr.append(n)
  twrite(sl,sr)
  wh=[]
  for i,field in enumerate(FIELDS):
   for fn,root,op,typ,fname in [(MUT[i],f'def:CALL@{MUT[i][:-2]}78:34:4','CALL','equipToolsDataMsg *','operator.new'),('00299cf4','const:0x0:4','-','undefined4','-'),('00299fae',f'def:LOAD@sw{i}:1:4','LOAD','undefined4','-'),('0029c20c',f'def:CALL@def{i}:1:4','CALL','undefined4','PVEDATA::equipToolsDataMsg::default_instance')]:
    wh.append({'format':FMT_W,'function_entry':fn,'function_name':'x','address_offset':f'0x{field:x}','value_root_token':root,'value_root_opcode':op,'value_root_type_display':typ,'factory_target_name':fname})
  twrite(wr,wh)
  fr=[]
  def flowrow(fn,addr,target,kind,off,idx='0',rt='call_arg',af='-'):
   return {'format':FMT_F,'function_entry':fn,'op_address':addr,'call_target_name':target,'call_kind':'CALL','arg_index':idx,'row_type':rt,'owner_parameter_proven':'1','arg_flow_kind':kind,'arg_owner_offset':off,'address_flow_kind':af}
  fr += [flowrow('00299dd0','00299dee','google::protobuf::Message::~Message','owner_address','0x0'),flowrow('00299dfc','00299e06','operator.delete','owner_address','0x0'),flowrow('00299f1c','00299f70','google::protobuf::internal::ReflectionOps::Merge','owner_address','0x0','1')]
  fr += [flowrow('00299e24','x','PVEDATA::equipToolsDataMsg::MergeFrom','owner_derived_unknown','-')]
  twrite(flow,fr)
  jwrite(cat,[{'format':FMT_C,'class_name':'PVEDATA::equipToolsDataMsg','receiver_vtable_id':'libcocos2dcpp.so:008f32b0','dispatch_slot_offset':'0x4','static_status':'single_function_memory_pointer','candidate_target_ids':['libcocos2dcpp.so:00299b1c'],'runtime_validation_status':'runtime_unobserved'}])
  for i,entry in enumerate(MUT):
   newaddr=entry[:-2]+'78'; rows=[{'function_entry':entry,'op_address':newaddr,'op_time':'34','opcode':'CALL','output':f'def:{newaddr}:34:4','inputs':'const:0x24:4','callee_entry':'0070e330','callee_name':'operator.new'},{'function_entry':entry,'op_address':entry[:-2]+'7e','op_time':'40','opcode':'CALL','output':'-','inputs':f'def:{newaddr}:34:4','callee_entry':'00299a30','callee_name':'PVEDATA::equipToolsDataMsg::equipToolsDataMsg'}];twrite(pc/f'pcode-{entry}.tsv',rows)
  rows=[];time=1
  for field in FIELDS:
   for base,sfx in [('register:register:00000020:4','a'),('register:register:00000024:4','b')]:
    p=f'def:p{field:x}{sfx}:1:4';c=f'def:c{field:x}{sfx}:1:4';l=f'def:l{field:x}{sfx}:1:4'
    rows += [{'function_entry':'00299fae','op_address':'x','op_time':str(time),'opcode':'PTRADD','output':p,'inputs':f'{base};const:0x{field:x}:4;const:0x1:4','callee_entry':'-','callee_name':'-'},{'function_entry':'00299fae','op_address':'x','op_time':str(time+1),'opcode':'CAST','output':c,'inputs':p,'callee_entry':'-','callee_name':'-'},{'function_entry':'00299fae','op_address':'x','op_time':str(time+2),'opcode':'LOAD','output':l,'inputs':f'const:0x1a1:4;{c}','callee_entry':'-','callee_name':'-'}];time+=3
   a=f'def:c{field:x}a:1:4';b=f'def:c{field:x}b:1:4';la=f'def:l{field:x}a:1:4';lb=f'def:l{field:x}b:1:4'
   rows += [{'function_entry':'00299fae','op_address':'x','op_time':str(time),'opcode':'STORE','output':'-','inputs':f'const:0x1a1:4;{a};{lb}','callee_entry':'-','callee_name':'-'},{'function_entry':'00299fae','op_address':'x','op_time':str(time+1),'opcode':'STORE','output':'-','inputs':f'const:0x1a1:4;{b};{la}','callee_entry':'-','callee_name':'-'}];time+=2
  twrite(pc/'pcode-00299fae.tsv',rows)
  out=t/'o';oq=t/'oq';det=t/'d'
  cmd=['python3',str(final),str(inf),str(q),str(sl),str(wr),str(flow),str(pc),str(cat),'--output',str(out),'--queries-output',str(oq),'--details-output',str(det),'--require-four']
  subprocess.run(cmd,check=True)
  ds=[json.loads(x) for x in det.read_text().splitlines() if x.strip()];assert len(ds)==4 and all(x['accepted'] for x in ds)
  fr.append(flowrow('bad','bad','external','owner_address','0x8'));twrite(flow,fr)
  r=subprocess.run(cmd);assert r.returncode!=0
 print('v25 protobuf nested receiver recovery smoke: pass')
if __name__=='__main__':main()
