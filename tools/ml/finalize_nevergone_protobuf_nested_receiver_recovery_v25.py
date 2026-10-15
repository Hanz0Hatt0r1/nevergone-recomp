#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,json
from collections import defaultdict
from pathlib import Path
from typing import Any

INF_FMT='NEVERGONE.StaticReceiverVtableInference/1'
Q_FMT='NEVERGONE.StaticReceiverVtableQuery/1'
SLICE_FMT='NEVERGONE.GhidraStructuralReceiverSlice/1'
WRITE_FMT='NEVERGONE.GhidraClassParameterFieldWrite/1'
FLOW_FMT='NEVERGONE.GhidraClassOwnerAliasFlow/1'
CAT_FMT='NEVERGONE.StaticVtableSlotCatalog/3'
DETAIL_FMT='NEVERGONE.ProtobufNestedReceiverRecoveryV25/1'
SOURCE='libcocos2dcpp.so:00299d90'
OWNER='PVEDATA::equipToolsListMsg'
NESTED='PVEDATA::equipToolsDataMsg'
VT='libcocos2dcpp.so:008f32b0'
SLOT='0x4'
FIELDS={0:0x8,1:0xc,2:0x10,3:0x14}
MUTABLE={0x8:'00299a66',0xc:'00299a88',0x10:'00299aaa',0x14:'00299acc'}
ALLOWED_WHOLE_OWNER={
 ('00299dd0','00299dee','google::protobuf::Message::~Message','CALL','0'),
 ('00299dfc','00299e06','operator.delete','CALL','0'),
 ('00299f1c','00299f70','google::protobuf::internal::ReflectionOps::Merge','CALL','1'),
}
ALLOWED_UNKNOWN_CALLEE_PREFIXES=(
 'PVEDATA::equipToolsDataMsg::MergeFrom',
 'google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual<PVEDATA::equipToolsDataMsg>',
 'google::protobuf::internal::WireFormatLite::WriteMessageNoVirtualToArray<PVEDATA::equipToolsDataMsg>',
 'google::protobuf::internal::WireFormat::SerializeUnknownFieldsToArray',
)

def jsonl(p:Path):
 return [json.loads(x) for x in p.read_text(encoding='utf-8').splitlines() if x.strip()]
def write_jsonl(p:Path,rows):
 p.parent.mkdir(parents=True,exist_ok=True);p.write_text(''.join(json.dumps(x,sort_keys=True)+'\n' for x in rows),encoding='utf-8')
def tsv(p:Path):
 with p.open(encoding='utf-8',newline='') as f:return list(csv.DictReader(f,delimiter='\t'))
def num(v:Any):
 if v in (None,'','-'): return None
 try:return int(str(v),0)
 except:return None

def validate(rows,fmt,p):
 for i,r in enumerate(rows,2):
  if r.get('format')!=fmt: raise ValueError(f'{p}:{i}: expected {fmt}')

def slice_fields(rows):
 out={};by=defaultdict(list)
 for r in rows:
  idx=num(r.get('call_index'))
  if idx is not None:by[idx].append(r)
 for idx,g in by.items():
  sums=[r for r in g if r.get('row_type')=='summary'];nodes=[r for r in g if r.get('row_type')=='node']
  if len(sums)!=1:continue
  s=sums[0];cs={num(r.get('constant_value')) for r in nodes};fs=sorted(x for x in cs if x in set(FIELDS.values()))
  out[idx]={'summary':s,'fields':fs,'nodes':nodes}
 return out

def pcode_rows(pdir:Path,entry:str):return tsv(pdir/f'pcode-{entry}.tsv')

def prove_mutable(pdir:Path,field:int,writes:list[dict[str,str]],reasons:list[str]):
 entry=MUTABLE[field];w=[r for r in writes if r.get('function_entry')==entry]
 if len(w)!=1:reasons.append(f'field_0x{field:x}_mutable_write_count_{len(w)}');return
 r=w[0]
 if r.get('factory_target_name')!='operator.new':reasons.append(f'field_0x{field:x}_mutable_not_operator_new')
 if (r.get('value_root_type_display') or '').replace(' ','')!='equipToolsDataMsg*':reasons.append(f'field_0x{field:x}_mutable_type_not_nested')
 pc=pcode_rows(pdir,entry);calls=[x for x in pc if x.get('opcode')=='CALL']
 news=[x for x in calls if x.get('callee_name')=='operator.new'];ctors=[x for x in calls if x.get('callee_name')=='PVEDATA::equipToolsDataMsg::equipToolsDataMsg']
 if len(news)!=1 or len(ctors)!=1:reasons.append(f'field_0x{field:x}_mutable_new_ctor_not_unique');return
 n,c=news[0],ctors[0]
 if n.get('inputs')!='const:0x24:4':reasons.append(f'field_0x{field:x}_operator_new_size_not_0x24')
 if n.get('output') not in (c.get('inputs') or '').split(';'):reasons.append(f'field_0x{field:x}_constructor_not_on_new_result')
 def normtok(x):return (x or '').replace('def:CALL@','def:')
 if normtok(r.get('value_root_token'))!=normtok(n.get('output')):reasons.append(f'field_0x{field:x}_store_not_new_result')

def prove_swap(pdir:Path,field:int,writes:list[dict[str,str]],reasons:list[str]):
 sw=[r for r in writes if r.get('function_entry')=='00299fae']
 if len(sw)!=1:reasons.append(f'field_0x{field:x}_swap_write_count_{len(sw)}');return
 pc=pcode_rows(pdir,'00299fae')
 def ins(r):return (r.get('inputs') or '').split(';')
 defs={r.get('output'):r for r in pc if r.get('output') not in (None,'','-')}
 def address_shape(tok,depth=0):
  if not tok or depth>6:return None
  r=defs.get(tok)
  if not r:return None
  if r.get('opcode') in ('CAST','COPY','INDIRECT'):
   xs=ins(r);return address_shape(xs[0] if xs else None,depth+1)
  if r.get('opcode')!='PTRADD':return None
  xs=ins(r)
  if len(xs)<3:return None
  try:off=int(xs[1].split(':')[1],16) if xs[1].startswith('const:0x') else None
  except:off=None
  return (xs[0],off)
 loads=[];stores=[]
 for r in pc:
  xs=ins(r)
  if r.get('opcode')=='LOAD' and len(xs)>=2 and address_shape(xs[1]) in {('register:register:00000020:4',field),('register:register:00000024:4',field)}:loads.append(r)
  if r.get('opcode')=='STORE' and len(xs)>=3 and address_shape(xs[1]) in {('register:register:00000020:4',field),('register:register:00000024:4',field)}:stores.append(r)
 if len(loads)!=2 or len(stores)!=2:reasons.append(f'field_0x{field:x}_swap_shape_not_2load_2store');return
 load_by_base={address_shape(ins(r)[1])[0]:r.get('output') for r in loads};store_by_base={address_shape(ins(r)[1])[0]:ins(r)[2] for r in stores}
 r0='register:register:00000020:4';r1='register:register:00000024:4'
 if set(load_by_base)!={r0,r1} or set(store_by_base)!={r0,r1}:reasons.append(f'field_0x{field:x}_swap_not_r0_r1_same_offset');return
 if store_by_base[r0]!=load_by_base[r1] or store_by_base[r1]!=load_by_base[r0]:reasons.append(f'field_0x{field:x}_swap_values_not_reciprocal_loads')

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('inference',type=Path);ap.add_argument('queries',type=Path);ap.add_argument('slice',type=Path);ap.add_argument('writes',type=Path);ap.add_argument('flow',type=Path);ap.add_argument('pcode_dir',type=Path);ap.add_argument('catalog',type=Path)
 ap.add_argument('--output',type=Path,required=True);ap.add_argument('--queries-output',type=Path,required=True);ap.add_argument('--details-output',type=Path,required=True);ap.add_argument('--require-four',action='store_true')
 a=ap.parse_args();inf=jsonl(a.inference);q=jsonl(a.queries);sl=tsv(a.slice);wr=tsv(a.writes);fl=tsv(a.flow);cat=jsonl(a.catalog)
 validate(inf,INF_FMT,a.inference);validate(q,Q_FMT,a.queries);validate(sl,SLICE_FMT,a.slice);validate(wr,WRITE_FMT,a.writes);validate(fl,FLOW_FMT,a.flow);validate(cat,CAT_FMT,a.catalog)
 by={(r['source_id'],r['call_index']):r for r in inf};qby={(r['source_id'],r['call_index']):dict(r) for r in q};sm=slice_fields(sl);wb=defaultdict(list)
 for r in wr:
  o=num(r.get('address_offset'))
  if o in FIELDS.values():wb[o].append(r)
 details=[];accepted=0
 whole={(r.get('function_entry'),r.get('op_address'),r.get('call_target_name'),r.get('call_kind'),r.get('arg_index')) for r in fl if r.get('owner_parameter_proven')=='1' and r.get('row_type')=='call_arg' and r.get('arg_flow_kind')=='owner_address' and num(r.get('arg_owner_offset'))==0 and not (r.get('call_target_name') or '').startswith(OWNER+'::')}
 unknown=[r for r in fl if r.get('owner_parameter_proven')=='1' and r.get('row_type')=='call_arg' and r.get('arg_flow_kind')=='owner_derived_unknown']
 unknown_bad=[r for r in unknown if not any((r.get('call_target_name') or '').startswith(p) for p in ALLOWED_UNKNOWN_CALLEE_PREFIXES)]
 field_addr=[r for r in fl if r.get('owner_parameter_proven')=='1' and r.get('row_type')=='call_arg' and r.get('arg_flow_kind')=='owner_address' and num(r.get('arg_owner_offset')) in FIELDS.values()]
 unknown_store=[r for r in fl if r.get('owner_parameter_proven')=='1' and r.get('row_type')=='store' and r.get('address_flow_kind')=='owner_derived_unknown']
 for idx,field in FIELDS.items():
  reasons=[];row=by.get((SOURCE,idx));s=sm.get(idx)
  if not row:reasons.append('inference_missing')
  else:
   if row.get('status')!='unresolved_receiver_class':reasons.append('status_not_unresolved_receiver_class')
   if str(row.get('dispatch_slot_offset')).lower()!=SLOT:reasons.append('slot_not_0x4')
  if (SOURCE,idx) in qby:reasons.append('query_already_exists')
  if not s:reasons.append('slice_missing')
  else:
   ss=s['summary']
   if ss.get('recovery_status')!='canonical_receiver' or num(ss.get('truncated_by_budget'))!=0 or num(ss.get('depth_truncation_count'))!=0 or num(ss.get('cycle_count'))!=0:reasons.append('slice_not_complete')
   if s['fields']!=[field]:reasons.append('slice_field_not_exact')
  fw=wb.get(field,[]);funcs=sorted(r.get('function_entry') for r in fw);expected=sorted([MUTABLE[field],'00299cf4','00299fae','0029c20c'])
  if funcs!=expected:reasons.append('direct_write_inventory_not_exact')
  nulls=[r for r in fw if (r.get('value_root_token') or '').startswith('const:0x0:') and r.get('function_entry')=='00299cf4']
  if len(nulls)!=1:reasons.append('shared_ctor_null_not_unique')
  defaults=[r for r in fw if r.get('function_entry')=='0029c20c' and r.get('factory_target_name')=='PVEDATA::equipToolsDataMsg::default_instance']
  if len(defaults)!=1:reasons.append('default_instance_write_not_unique')
  prove_mutable(a.pcode_dir,field,fw,reasons);prove_swap(a.pcode_dir,field,fw,reasons)
  if field_addr:reasons.append('target_field_address_escape_present')
  if unknown_store:reasons.append('owner_unknown_store_present')
  if whole!=ALLOWED_WHOLE_OWNER:reasons.append('whole_owner_frontier_not_exactly_allowlisted')
  if unknown_bad:reasons.append('unexpected_owner_derived_unknown_call')
  cr=[r for r in cat if r.get('class_name')==NESTED and r.get('receiver_vtable_id')==VT and str(r.get('dispatch_slot_offset')).lower()==SLOT]
  target=None;runtime='runtime_unobserved'
  if len(cr)!=1:reasons.append('catalog_class_vtable_slot_not_unique')
  else:
   c=cr[0];runtime=str(c.get('runtime_validation_status') or 'runtime_unobserved');ts=c.get('candidate_target_ids')
   if c.get('static_status')!='single_function_memory_pointer' or not isinstance(ts,list) or len(ts)!=1:reasons.append('catalog_target_not_unique')
   else:target=ts[0]
   if target!='libcocos2dcpp.so:00299b1c':reasons.append('catalog_slot4_not_nested_destructor')
   if c.get('runtime_validation_status')=='runtime_mismatch':reasons.append('catalog_runtime_mismatch')
  ok=not reasons and target is not None
  proof={'format':DETAIL_FMT,'source_id':SOURCE,'call_index':idx,'call_address':row.get('call_address') if row else None,'owner_class':OWNER,'field_offset':f'0x{field:x}','dispatch_slot_offset':SLOT,'receiver_class_name':NESTED,'receiver_vtable_id':VT,'candidate_target_ids':[target] if target else [],'accepted':ok,'direct_write_functions':funcs,'whole_owner_frontier':sorted(whole),'owner_derived_unknown_call_count':len(unknown),'unexpected_owner_derived_unknown_call_count':len(unknown_bad),'target_field_address_escape_count':len(field_addr),'unknown_store_count':len(unknown_store),'proof_kind':'exact_generated_protobuf_nested_field_lifecycle','reasons':reasons,'affects_routing':False}
  details.append(proof)
  if ok:
   accepted+=1;row['class_name']=NESTED;row['class_match_kind']='exact_generated_protobuf_nested_field_lifecycle_v25';row['class_evidence_source']='v25_exact_native_nested_field_lifecycle_and_catalog_slot';row['candidate_receiver_class_names']=[NESTED];row['candidate_receiver_vtable_ids']=[VT];row['receiver_vtable_id']=VT;row['status']='resolved_unique_vtable';row['protobuf_nested_receiver_recovery_v25']=proof
   qby[(SOURCE,idx)]={'format':Q_FMT,'source_id':SOURCE,'call_index':idx,'call_address':row.get('call_address'),'receiver_vtable_id':VT,'dispatch_slot_offset':SLOT,'receiver_class_name':NESTED,'receiver_class_match_kind':row['class_match_kind'],'receiver_class_evidence_source':row['class_evidence_source'],'receiver_type_name':row.get('receiver_type_name'),'receiver_type_display':row.get('receiver_type_display'),'runtime_validation_status':runtime,'evidence_ref':f'{SOURCE}#call={idx}','affects_routing':False}
  elif row is not None:row['protobuf_nested_receiver_recovery_v25']=proof
 write_jsonl(a.output,inf);write_jsonl(a.queries_output,list(qby.values()));write_jsonl(a.details_output,details)
 print('format:',DETAIL_FMT);print('accepted:',accepted)
 for d in details:print(d['source_id'],'call',d['call_index'],'field',d['field_offset'],'accepted=',d['accepted'],'reasons=',d['reasons'])
 print('affects routing: no')
 if a.require_four and accepted!=4:return 3
 return 0
if __name__=='__main__':raise SystemExit(main())
