#!/usr/bin/env python3
"""Summarize structural LOAD shapes newly exposed by receiver safety v20."""
from __future__ import annotations
import argparse,csv,json
from collections import defaultdict
from pathlib import Path
from typing import Any
F="NEVERGONE.GhidraStructuralReceiverSlice/1";CF="NEVERGONE.GhidraAddressClassification/1";OUT="NEVERGONE.NewlyUnresolvedFieldShapes/1"
TARGETS={"libcocos2dcpp.so:00299d90":4,"libcocos2dcpp.so:002a651c":2}
def tsv(p):
 with p.open(encoding='utf-8',newline='') as f:return list(csv.DictReader(f,delimiter='\t'))
def n(v):
 try:return int(str(v),0)
 except:return None
def d(v,x=-1):
 try:return int(str(v),10)
 except:return x
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('slice299',type=Path);ap.add_argument('slice651',type=Path);ap.add_argument('identities',type=Path);ap.add_argument('--details-output',type=Path);a=ap.parse_args()
 slices={"libcocos2dcpp.so:00299d90":tsv(a.slice299),"libcocos2dcpp.so:002a651c":tsv(a.slice651)};ids=tsv(a.identities)
 for p,rows,fmt in [(a.slice299,slices['libcocos2dcpp.so:00299d90'],F),(a.slice651,slices['libcocos2dcpp.so:002a651c'],F),(a.identities,ids,CF)]:
  for i,r in enumerate(rows,2):
   if r.get('format')!=fmt:raise ValueError(f'{p}:{i}: expected {fmt}')
 idby={r.get('address'):r for r in ids};out=[]
 for src,expected in TARGETS.items():
  entry=src.rsplit(':',1)[1];rows=slices[src]; summaries=[r for r in rows if r.get('row_type')=='summary']
  if len(summaries)!=expected:raise ValueError(f'{src}: expected {expected} summaries, got {len(summaries)}')
  by=defaultdict(list)
  for r in rows:
   ci=d(r.get('call_index'))
   if ci>=0:by[ci].append(r)
  ident=idby.get(entry) or {}; fn=ident.get('function_at_name')
  for ci in range(expected):
   rs=by[ci];sm=[r for r in rs if r.get('row_type')=='summary']
   if len(sm)!=1:raise ValueError(f'{src} call {ci}: summary not unique')
   nodes=[r for r in rs if r.get('row_type')=='node'];root=next((r for r in nodes if d(r.get('node_id'))==0),{})
   const=sorted({x for x in (n(r.get('constant_value')) for r in nodes) if x is not None})
   plausible=[x for x in const if 0x20<=x<=0x1000]
   out.append({'format':OUT,'source_id':src,'function_entry':entry,'function_name':fn,'call_index':ci,'call_address':sm[0].get('call_address'),'dispatch_slot_offset':sm[0].get('slot_offset'),'recovery_status':sm[0].get('recovery_status'),'root_opcode':root.get('defining_opcode'),'constant_values':[f'0x{x:x}' for x in const],'plausible_field_offsets':[f'0x{x:x}' for x in plausible],'receiver_class_proven':False,'affects_routing':False})
 print('format:',OUT);print('rows:',len(out))
 for r in out:print(r['source_id'],'call',r['call_index'],'fn=',r['function_name'],'slot=',r['dispatch_slot_offset'],'root=',r['root_opcode'],'fields=',r['plausible_field_offsets'],'status=',r['recovery_status'])
 if a.details_output:
  a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text(''.join(json.dumps(x,sort_keys=True)+'\n' for x in out),encoding='utf-8');print('details output:',a.details_output)
 print('receiver class proven: no');print('affects routing: no')
if __name__=='__main__':main()
