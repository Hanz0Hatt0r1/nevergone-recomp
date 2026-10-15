#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv
from pathlib import Path
FMT='NEVERGONE.GhidraExactParameterFieldFactoryStore/1'
FACTORIES={'cocos2d::CCSprite::create','cocos2d::CCSprite::createWithSpriteFrame','cocos2d::CCSprite::createWithSpriteFrameName'}

def rows(p):
    with p.open(encoding='utf-8',newline='') as f:return list(csv.DictReader(f,delimiter='\t'))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--helper',action='append',nargs=2,metavar=('ENTRY','TSV'),required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args(); out=[]
    for entry,sp in a.helper:
        p=Path(sp); rs=rows(p)
        for n,r in enumerate(rs,2):
            if r.get('format')!=FMT:raise ValueError(f'{p}:{n}: expected {FMT}')
        cand=[r for r in rs if r.get('factory_target_name') in FACTORIES and r.get('base_parameter_index') not in {'','-'}]
        params=sorted({int(r['base_parameter_index']) for r in cand})
        if len(params)!=1:
            raise SystemExit(f'{entry}: expected one CCSprite factory destination parameter, got {params}')
        out.append({'source_id':f'libcocos2dcpp.so:{entry}','source_entry':entry,'source_call_index':'0','parameter_index':str(params[0]),'dispatch_slot_offset':'-','expected_direct_caller_functions':'0','expected_matched_caller_calls':'0'})
    a.output.parent.mkdir(parents=True,exist_ok=True)
    fields=['source_id','source_entry','source_call_index','parameter_index','dispatch_slot_offset','expected_direct_caller_functions','expected_matched_caller_calls']
    with a.output.open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields,delimiter='\t');w.writeheader();w.writerows(out)
    for r in out: print(r['source_entry'],'factory destination parameter',r['parameter_index'])
    print('plan:',a.output)
    return 0
if __name__=='__main__':raise SystemExit(main())
