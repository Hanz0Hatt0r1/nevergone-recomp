#!/usr/bin/env python3
"""Audit the complete 19-method ControlsButton owner-field alias probe."""
from __future__ import annotations
import argparse,csv,json
from collections import Counter,defaultdict
from pathlib import Path

PLAN='NEVERGONE.ControlsButtonOwnerFieldProbePlan/1';GH='NEVERGONE.GhidraControlsButtonOwnerFieldAliasInventory/1';OUT='NEVERGONE.ControlsButtonOwnerFieldAliasAudit/1'
EXPECTED={
 '002a8494':'0x104','002a849a':'0x108','002a84a8':'0x10c','002a84cc':'0x104','002a84ea':'0x108','002a8510':'0x10c',
 '002a8596':'0x104','002a859e':'0x108','002a85a4':'0x10c','002a85c4':'0x104','002a85de':'0x108',
}
FIELDS={'0x104','0x108','0x10c'}
def j(p):return [json.loads(x) for x in p.read_text(encoding='utf-8').splitlines() if x.strip()]
def t(p):
 with p.open(encoding='utf-8',newline='') as h:return list(csv.DictReader(h,delimiter='\t'))
def integer(x):
 try:return int(str(x))
 except:return -1

def audit(plan,manifest,rows):
 if len(plan)!=1 or plan[0].get('format')!=PLAN or plan[0].get('controlsbutton_symbol_count')!=21 or plan[0].get('controlsbutton_unique_method_count')!=19 or plan[0].get('direct_target_field_store_count')!=11:raise ValueError('native ControlsButton plan mismatch')
 entries=[r.get('function_entry') for r in manifest]
 if len(entries)!=19 or len(set(entries))!=19 or sorted(entries)!=plan[0].get('controlsbutton_method_entries'):raise ValueError('method manifest mismatch')
 if any(r.get('format')!=GH or r.get('function_entry') not in entries or r.get('row_type') not in {'summary','store','call'} for r in rows):raise ValueError('Ghidra row identity mismatch')
 by=defaultdict(list)
 for r in rows:by[r['function_entry']].append(r)
 reasons=[];stores=[];calls=[];failed=[]
 for e in entries:
  group=by[e];s=[r for r in group if r['row_type']=='summary'];ss=[r for r in group if r['row_type']=='store'];cc=[r for r in group if r['row_type']=='call']
  if len(s)!=1:reasons.append('summary_not_unique:'+e);continue
  x=s[0]
  if x.get('decompile_status')!='ok':failed.append(e);reasons.append('method_not_decompiled:'+e);continue
  if integer(x.get('store_count'))!=len(ss) or integer(x.get('call_count'))!=len(cc):reasons.append('operation_count_mismatch:'+e)
  unknown=sum(r.get('address_offset')=='-' or r.get('address_parameter_index')=='-' or r.get('address_kind','').startswith('opaque_') or r.get('address_kind')=='phi_ambiguous' for r in ss)
  if integer(x.get('unknown_store_count'))!=unknown:reasons.append('unknown_store_count_mismatch:'+e)
  p0=sum(r.get('param0_arg_indices') not in ('-', '', None) for r in cc)
  if integer(x.get('param0_call_count'))!=p0:reasons.append('param0_call_count_mismatch:'+e)
  if any(r.get('op_address') in ('-',None,'') or integer(r.get('op_time'))<0 for r in ss+cc):reasons.append('operation_identity_missing:'+e)
  stores+=ss;calls+=cc
 direct=[r for r in stores if r.get('address_parameter_index')=='0' and r.get('address_offset') in FIELDS]
 mapping={r.get('op_address'):r.get('address_offset') for r in direct}
 if len(mapping)!=len(direct):reasons.append('duplicate_direct_target_store')
 if mapping!=EXPECTED:reasons.append('direct_target_field_writer_set_changed')
 unknown=[r for r in stores if r.get('address_offset')=='-' or r.get('address_parameter_index')=='-' or r.get('address_kind','').startswith('opaque_') or r.get('address_kind')=='phi_ambiguous']
 p0calls=[r for r in calls if r.get('param0_arg_indices') not in ('-', '', None)]
 offsets=Counter()
 for r in p0calls:
  for off in (r.get('param0_arg_offsets') or '').split(','):
   if off and off!='-':offsets[off]+=1
 return {'format':OUT,'status':'bounded_controlsbutton_owner_probe_complete' if not reasons else 'bounded_controlsbutton_owner_probe_incomplete','methods_expected':19,'methods_decompiled':19-len(failed),'all_stores':len(stores),'all_calls':len(calls),'direct_target_field_writes':mapping,'unknown_store_count':len(unknown),'parameter_zero_call_count':len(p0calls),'parameter_zero_argument_offsets':dict(sorted(offsets.items())),'unknown_store_samples':[{'function_entry':r['function_entry'],'op_address':r['op_address'],'address_kind':r.get('address_kind'),'address_root_token':r.get('address_root_token'),'address_offset':r.get('address_offset')} for r in unknown[:60]],'parameter_zero_call_samples':[{'function_entry':r['function_entry'],'op_address':r['op_address'],'call_kind':r.get('call_kind'),'call_target_token':r.get('call_target_token'),'arg_indices':r.get('param0_arg_indices'),'arg_offsets':r.get('param0_arg_offsets'),'unknown_args':r.get('unknown_arg_indices')} for r in p0calls[:80]],'reasons':sorted(set(reasons)),'owner_fields_stable':False,'receiver_class_proven':False,'affects_routing':False}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('native_plan_jsonl',type=Path);p.add_argument('methods_tsv',type=Path);p.add_argument('ghidra_tsv',type=Path);p.add_argument('--details-output',type=Path,required=True);p.add_argument('--require-complete',action='store_true');a=p.parse_args();r=audit(j(a.native_plan_jsonl),t(a.methods_tsv),t(a.ghidra_tsv));a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text(json.dumps(r,sort_keys=True)+'\n',encoding='utf-8');print('format:',OUT);print('status:',r['status']);print('methods:',r['methods_decompiled'],'/',r['methods_expected']);print('STORE/CALL:',r['all_stores'],'/',r['all_calls']);print('unknown STOREs:',r['unknown_store_count']);print('parameter-zero calls:',r['parameter_zero_call_count']);print('reasons:',r['reasons']);print('owner field stability: not yet proven; affects routing: no');print('details:',a.details_output);return 2 if a.require_complete and r['reasons'] else 0
if __name__=='__main__':raise SystemExit(main())
