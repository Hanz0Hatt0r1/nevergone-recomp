#!/usr/bin/env python3
"""Prove packaged stability of Controls+0x124 (CCJoyStick) and +0x174 (CCSprite)."""
from __future__ import annotations

import argparse,csv,io,json,tarfile
from pathlib import Path
from typing import Any

FORMAT="NEVERGONE.ControlsAuxFieldStability/1"
SHA="94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
FIELD_AUDIT="NEVERGONE.ControlsFieldWriteAliasAudit/1"
STABILITY="NEVERGONE.ControlsButtonPackagedFieldStability/1"
CANDIDATE="NEVERGONE.Slot80FactoryFieldCandidate/1"
TARGETS={
 "0x124":{"class":"CCJoyStick","vtable":"libcocos2dcpp.so:008f4228","target":"libcocos2dcpp.so:00525d2e","store":"002a8066"},
 "0x174":{"class":"cocos2d::CCSprite","vtable":"libcocos2dcpp.so:009363d8","target":"libcocos2dcpp.so:00549f1e","store":"002a80e0"},
}
CALLS={
 ("libcocos2dcpp.so:002a74c0",3):("002a752a","0x124"),
 ("libcocos2dcpp.so:002a7696",0):("002a76b6","0x124"),
 ("libcocos2dcpp.so:002a7696",1):("002a76c4","0x174"),
}
MEMBERS={"ghidra":"evidence/controls-field-write-aliases.tsv","audit":"controls-field-write-alias-audit.jsonl"}


def require(ok:bool,msg:str)->None:
 if not ok: raise ValueError(msg)

def jlines(s:str)->list[dict[str,Any]]: return [json.loads(x) for x in s.splitlines() if x.strip()]
def tsv(s:str)->list[dict[str,str]]: return list(csv.DictReader(io.StringIO(s),delimiter='\t'))

def archive(path:Path)->dict[str,str]:
 with tarfile.open(path,'r:gz') as t:
  return {k:t.extractfile(t.getmember(v)).read().decode() for k,v in MEMBERS.items()}

def read(path:Path)->list[dict[str,Any]]: return jlines(path.read_text(encoding='utf-8'))

def main()->int:
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('field_alias_archive',type=Path);p.add_argument('gated_v14_stability_jsonl',type=Path)
 p.add_argument('slot80_factory_candidates_jsonl',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 ar=archive(a.field_alias_archive); audit=jlines(ar['audit']); require(len(audit)==1,'field audit row count')
 fa=audit[0]; require(fa.get('format')==FIELD_AUDIT and fa.get('status')=='bounded_controls_methods_scanned_with_alias_caveat' and
  fa.get('controls_methods_decompiled')==38 and fa.get('reasons')==[] and fa.get('ghidra_stores_in_controls_methods')==167 and
  fa.get('ghidra_unknown_address_stores')==46 and fa.get('affects_routing') is False,'field audit contract changed')
 rows=tsv(ar['ghidra']); stores=[r for r in rows if r.get('row_type')=='store' and r.get('base_parameter_index')=='0']
 for field,data in TARGETS.items():
  m=[r for r in stores if r.get('address_offset')==field]
  require(len(m)==1 and m[0].get('function_entry')=='002a7e2c' and m[0].get('op_address')==data['store'],f'{field} direct writer set changed')
 st=read(a.gated_v14_stability_jsonl); ss=[r for r in st if r.get('format')==STABILITY and r.get('record_type')=='summary'];require(len(ss)==1,'v14 stability summary')
 s=ss[0]; require(s.get('elf_sha256')==SHA and s.get('scope')=='exact_packaged_apk_normal_code_execution' and
  s.get('local_stores_classified')==46 and s.get('unadjusted_controls_calls_classified')==35 and
  s.get('retained_button_and_joystick_alias_paths_closed') is True and s.get('scheduler_action_callback_paths_closed') is True and
  s.get('parent_native_dangerous_write_through_excluded') is True and s.get('packaged_static_script_engine_setter_reachable') is False and
  s.get('packaged_dlsym_set_script_engine_lookup_excluded') is True and s.get('external_full_this_static_frontier_closed') is True and
  s.get('external_full_this_sites_closed')==14 and s.get('known_external_frontier_writes_receiver_fields') is False and
  s.get('whole_program_alias_writes_excluded') is False and s.get('affects_routing') is False,'gated v14 closure changed')
 cand=read(a.slot80_factory_candidates_jsonl); by={(r.get('source_id'),r.get('call_index')):r for r in cand if (r.get('source_id'),r.get('call_index')) in CALLS}
 require(set(by)==set(CALLS),'aux receiver candidates missing')
 out=[{'format':FORMAT,'record_type':'summary','elf_sha256':SHA,'scope':'exact_packaged_apk_normal_code_execution','target_fields':['0x124','0x174'],
       'controls_aux_fields_stable':True,'external_full_this_static_frontier_closed':True,'whole_program_alias_writes_excluded':False,'affects_routing':False}]
 for k,(address,field) in CALLS.items():
  r=by[k]; meta=TARGETS[field]
  require(r.get('format')==CANDIDATE and r.get('call_address')==address and r.get('owner_class')=='Controls' and r.get('owner_field_offset')==field and
   r.get('factory_class_candidate')==meta['class'] and r.get('candidate_receiver_vtable_id')==meta['vtable'] and r.get('candidate_target_id')==meta['target'] and
   r.get('status')=='construction_linked_target_candidate' and r.get('affects_routing') is False,f'candidate changed {k}')
  out.append({'format':FORMAT,'record_type':'receiver_field','source_id':k[0],'call_index':k[1],'call_address':address,'dispatch_slot_offset':'0x80',
   'receiver_field_offset':field,'receiver_class_name':meta['class'],'receiver_vtable_id':meta['vtable'],'slot_target_id':meta['target'],
   'packaged_field_stability_proven':True,'external_full_this_static_frontier_closed':True,'whole_program_alias_writes_excluded':False,'affects_routing':False})
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in out),encoding='utf-8')
 print('format:',FORMAT);print('stable auxiliary Controls fields: 2/2');print('receiver calls covered: 3');print('whole-program alias exclusion: no');print('output:',a.output);return 0
if __name__=='__main__':raise SystemExit(main())
