#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,json
from collections import defaultdict
from pathlib import Path
from typing import Any
STORE_FMT='NEVERGONE.GhidraExactParameterFieldFactoryStore/1'
CALLER_FMT='NEVERGONE.GhidraCallerArgumentSubtree/1'
CAT_FMT='NEVERGONE.StaticVtableSlotCatalog/3'
OUT_FMT='NEVERGONE.BeatenEffectHelperFieldProvenanceV26/1'
FACTORIES={'cocos2d::CCSprite::create','cocos2d::CCSprite::createWithSpriteFrame','cocos2d::CCSprite::createWithSpriteFrameName'}
TARGETS={0x128,0x12c,0x130,0x134,0x138,0x14c,0x150,0x154,0x158}
HELPERS={'002a3a20':'createBeatenBloodPointEffect02','002a3c74':'createBeatenBloodPointEffect'}
OWNER_CALLEES={
 '00526124':'cocos2d::CCNode::removeAllChildrenWithCleanup',
 '005353f8':'cocos2d::CCLayer::~CCLayer',
 '00535204':'cocos2d::CCLayer::CCLayer',
 '00525dac':'cocos2d::CCNode::addChild',
 '0051f3c8':'cocos2d::CCCallFunc::create',
}

def tsv(p):
    with Path(p).open(encoding='utf-8',newline='') as f:return list(csv.DictReader(f,delimiter='\t'))
def num(v):
    if v in (None,'','-'):return None
    try:return int(str(v),0)
    except:return None
def validate(rs,p,fmt):
    for i,r in enumerate(rs,2):
        if r.get('format')!=fmt:raise ValueError(f'{p}:{i}: expected {fmt}')

def child_map(nodes):
    c=defaultdict(list)
    for r in nodes:
        pid=num(r.get('parent_node_id'))
        if pid is not None and pid>=0:c[pid].append(r)
    return c

def affine(nodes):
    if not nodes:return None
    by={num(r.get('node_id')):r for r in nodes if num(r.get('node_id')) is not None}; ch=child_map(nodes)
    root=by.get(0)
    if root is None:return None
    seen=set()
    def walk(r):
        rid=num(r.get('node_id'))
        if rid in seen:return None
        seen.add(rid)
        pd=num(r.get('pointer_depth'))
        pt=(r.get('pointee_type_display') or '').strip()
        td=(r.get('type_display') or '').replace(' ','')
        op=r.get('defining_opcode') or '-'
        cv=num(r.get('constant_value'))
        kids=sorted(ch.get(rid,[]),key=lambda x:num(x.get('input_index')) if num(x.get('input_index')) is not None else 99)
        if cv is not None:return ('const',cv)
        if (pd==1 and pt=='BeatenEffect') or td=='BeatenEffect*':return ('owner',0)
        if op in {'COPY','CAST','INDIRECT'} and len(kids)>=1:return walk(kids[0])
        if op=='MULTIEQUAL' and kids:
            vals=[walk(k) for k in kids]
            return vals[0] if vals and all(v==vals[0] and v is not None for v in vals) else None
        if op in {'PTRSUB','INT_ADD','INT_SUB'} and len(kids)>=2:
            a,b=walk(kids[0]),walk(kids[1])
            if a and a[0]=='owner' and b and b[0]=='const':return ('owner',a[1]+(b[1] if op!='INT_SUB' else -b[1]))
            if op=='INT_ADD' and b and b[0]=='owner' and a and a[0]=='const':return ('owner',b[1]+a[1])
            return None
        if op=='PTRADD' and len(kids)>=3:
            a,b,s=walk(kids[0]),walk(kids[1]),walk(kids[2])
            if a and a[0]=='owner' and b and b[0]=='const' and s and s[0]=='const':return ('owner',a[1]+b[1]*s[1])
            return None
        return None
    return walk(root)

def helper_result(entry,store_rows,caller_rows):
    validate(store_rows,entry,STORE_FMT);validate(caller_rows,'callers',CALLER_FMT)
    fac=[r for r in store_rows if r.get('factory_target_name') in FACTORIES and num(r.get('base_parameter_index')) is not None]
    params=sorted({num(r.get('base_parameter_index')) for r in fac}); reasons=[]
    if len(params)!=1: reasons.append('factory_destination_parameter_not_unique')
    param=params[0] if len(params)==1 else None
    relevant=[r for r in store_rows if param is not None and num(r.get('base_parameter_index'))==param]
    other_nonnull=[]
    for r in relevant:
        root=r.get('value_root_token') or ''
        if root.startswith('const:0x0:'):continue
        if r.get('factory_target_name') in FACTORIES:continue
        other_nonnull.append([r.get('op_address'),r.get('address_offset'),root,r.get('factory_target_name')])
    if other_nonnull:reasons.append('helper_other_nonnull_store_through_factory_parameter')
    summaries=[r for r in caller_rows if r.get('row_type')=='summary' and (r.get('source_entry') or '').lower()==entry]
    if len(summaries)!=1:
        reasons.append('caller_summary_not_unique')
        summary=summaries[0] if summaries else {}
    else:
        summary=summaries[0]
    direct=num(summary.get('direct_caller_functions')) or 0;failed=num(summary.get('caller_functions_failed')) or 0;withm=num(summary.get('callers_with_matched_calls')) or 0;matched=num(summary.get('matched_caller_calls')) or 0
    coverage=(direct>0 and failed==0 and withm==direct and matched>0)
    if not coverage:reasons.append('direct_caller_coverage_incomplete')
    groups=defaultdict(list)
    for r in caller_rows:
        if r.get('row_type')=='node' and (r.get('source_entry') or '').lower()==entry:
            groups[(r.get('caller_entry'),r.get('caller_call_site'))].append(r)
    caller_offsets=[];unresolved=[]
    for k,rs in sorted(groups.items()):
        v=affine(rs)
        if not v or v[0]!='owner':unresolved.append(k)
        else: caller_offsets.append(v[1])
    if unresolved:reasons.append('caller_argument_not_exact_beateneffect_affine')
    final=[]
    if not unresolved:
        for r in fac:
            so=num(r.get('address_offset'))
            if so is None:continue
            for co in caller_offsets:final.append(co+so)
    finalset=sorted(set(final))
    if finalset and not set(finalset)<=TARGETS:reasons.append('factory_write_reaches_unexpected_owner_offset')
    candidate=bool(fac) and coverage and not other_nonnull and not unresolved and bool(finalset) and set(finalset)<=TARGETS
    return {'helper_entry':entry,'helper_name':HELPERS[entry],'factory_destination_parameter':param,
      'factory_store_offsets':sorted({num(r.get('address_offset')) for r in fac if num(r.get('address_offset')) is not None}),
      'factory_targets':sorted({r.get('factory_target_name') for r in fac}),
      'direct_caller_functions':direct,'matched_caller_calls':matched,'caller_owner_offsets':sorted(set(caller_offsets)),
      'final_beateneffect_offsets':[f'0x{x:x}' for x in finalset],'field_factory_proven':candidate,'reasons':sorted(set(reasons)),
      'other_nonnull_stores':other_nonnull}

def callee_result(entry,rs):
    validate(rs,entry,STORE_FMT)
    p0=[r for r in rs if num(r.get('base_parameter_index'))==0]
    target=[]
    for r in p0:
        off=num(r.get('address_offset'))
        item=[r.get('op_address'),f'0x{off:x}' if off is not None else None,r.get('value_root_token'),r.get('factory_target_name')]
        if off in TARGETS:target.append(item)
    return {'callee_entry':entry,'callee_name':OWNER_CALLEES[entry],'parameter0_store_count':len(p0),'target_field_writes':target,'target_field_clean':not target}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--helper-store',action='append',nargs=2,metavar=('ENTRY','TSV'),required=True)
    ap.add_argument('--callers',type=Path,required=True)
    ap.add_argument('--owner-callee-store',action='append',nargs=2,metavar=('ENTRY','TSV'),required=True)
    ap.add_argument('--catalog',type=Path,required=True)
    ap.add_argument('--details-output',type=Path)
    a=ap.parse_args(); callers=tsv(a.callers)
    helpers={e:helper_result(e.lower(),tsv(p),callers) for e,p in a.helper_store}
    callees={e.lower():callee_result(e.lower(),tsv(p)) for e,p in a.owner_callee_store}
    cat=[json.loads(x) for x in a.catalog.read_text(encoding='utf-8').splitlines() if x.strip()]
    for i,r in enumerate(cat,1):
        if r.get('format')!=CAT_FMT:raise ValueError(f'{a.catalog}:{i}: expected {CAT_FMT}')
    slot=[r for r in cat if r.get('class_name')=='BeatenEffect' and str(r.get('dispatch_slot_offset')).lower()=='0xbc']
    slot_ok=len(slot)==1 and slot[0].get('candidate_target_ids')==['libcocos2dcpp.so:00525dac']
    field_union=sorted({int(x,16) for h in helpers.values() for x in h['final_beateneffect_offsets']})
    details={'format':OUT_FMT,'helpers':helpers,'whole_owner_callee_writes':callees,'slot_0xbc_addchild_exact':slot_ok,
      'proven_factory_fields':[f'0x{x:x}' for x in field_union if any(h['field_factory_proven'] and f'0x{x:x}' in h['final_beateneffect_offsets'] for h in helpers.values())],
      'all_known_whole_owner_callees_target_field_clean':all(r['target_field_clean'] for r in callees.values()),'affects_routing':False}
    print('format:',OUT_FMT)
    for e in sorted(helpers):
        h=helpers[e];print(e,h['helper_name'],'proven=',h['field_factory_proven'],'param=',h['factory_destination_parameter'],'caller_offsets=',h['caller_owner_offsets'],'final=',h['final_beateneffect_offsets'],'reasons=',h['reasons'])
    print('slot 0xbc exact addChild:',slot_ok)
    print('known whole-owner callees target-field clean:',details['all_known_whole_owner_callees_target_field_clean'])
    for e in sorted(callees):print(e,callees[e]['callee_name'],'target_field_writes=',callees[e]['target_field_writes'])
    print('proven factory fields:',details['proven_factory_fields'])
    if a.details_output:
        a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text(json.dumps(details,sort_keys=True)+'\n',encoding='utf-8');print('details:',a.details_output)
    print('affects routing: no')
    return 0
if __name__=='__main__':raise SystemExit(main())
