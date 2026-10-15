#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
AUD=ROOT/'tools/ml/audit_nevergone_residual_owner_field_writes_v21.py'
FIELDS=['format','class_name','function_entry','function_name','op_address','op_time','address_kind','address_base_token','address_offset','base_parameter_index','value_token','value_root_token','value_root_opcode','value_root_type_display','value_root_parameter_index','value_root_parameter_type_display','factory_target_entry','factory_target_name']

def wt(p,rows):
    with p.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=FIELDS,delimiter='\t'); w.writeheader(); w.writerows(rows)

def row(cls,off,root='CALL',factory='00549614',name='cocos2d::CCSprite::create',ptype='-',pidx='-'):
    return {'format':'NEVERGONE.GhidraClassParameterFieldWrite/1','class_name':cls,'function_entry':'00100000','function_name':cls+'::init','op_address':'00100010','op_time':'1','address_kind':'base_plus_constant','address_base_token':'register:0:4','address_offset':hex(off),'base_parameter_index':'0','value_token':'x','value_root_token':'def:CALL@1:1:4' if root=='CALL' else ('const:0x0:4' if root=='-' else 'input:register:4:4'),'value_root_opcode':root,'value_root_type_display':'cocos2d::CCSprite *' if root=='CALL' else ptype,'value_root_parameter_index':pidx,'value_root_parameter_type_display':ptype,'factory_target_entry':factory if root=='CALL' else '-','factory_target_name':name if root=='CALL' else '-'}

def main():
  with tempfile.TemporaryDirectory() as td:
    t=Path(td); b=t/'b.tsv'; j=t/'j.tsv'; d=t/'d.jsonl'
    beat=[row('BeatenEffect',x) for x in (0x128,0x12c,0x130,0x134,0x138,0x14c,0x150,0x154,0x158)]
    joy=[row('CCJoyStick',0x14c,root='-',factory='-',name='-',ptype='cocos2d::CCNode *',pidx='1')]
    wt(b,beat); wt(j,joy)
    p=subprocess.run([sys.executable,str(AUD),str(b),str(j),'--details-output',str(d),'--require-observed-targets'],text=True,capture_output=True)
    assert p.returncode==0,p.stdout+p.stderr
    rows=[json.loads(x) for x in d.read_text().splitlines() if x.strip()]
    assert len(rows)==10
    bb=[x for x in rows if x['owner_class']=='BeatenEffect']
    assert all(x['root_kind_counts']=={'direct_call_return':1} for x in bb)
    jj=[x for x in rows if x['owner_class']=='CCJoyStick'][0]
    assert jj['root_kind_counts']=={'function_parameter':1}
    assert jj['parameter_sources']==[{'index':1,'type_display':'cocos2d::CCNode *'}]
    wt(b,beat[:-1])
    p=subprocess.run([sys.executable,str(AUD),str(b),str(j),'--require-observed-targets'],text=True,capture_output=True)
    assert p.returncode!=0
  print('residual owner field writes v21 smoke: ok'); return 0
if __name__=='__main__': raise SystemExit(main())
