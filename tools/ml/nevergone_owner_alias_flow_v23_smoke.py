#!/usr/bin/env python3
from __future__ import annotations
import csv,json,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
AUD=ROOT/'tools/ml/audit_nevergone_owner_alias_flow_v23.py'
FIELDS=['format','row_type','class_name','function_entry','function_name','owner_parameter_proven','owner_parameter_type','owner_parameter_name','op_address','op_time','call_kind','call_target_entry','call_target_name','arg_index','arg_token','arg_flow_kind','arg_owner_offset','address_token','address_flow_kind','address_owner_offset','value_token','store_count','call_count','owner_address_arg_count','owner_field_value_arg_count','owner_derived_unknown_arg_count','owner_derived_unknown_store_count']
def wt(p,rows):
    with p.open('w',newline='',encoding='utf-8') as f:w=csv.DictWriter(f,fieldnames=FIELDS,delimiter='\t');w.writeheader();w.writerows(rows)
def base(cls,row_type='summary',owner='1'):
    return {k:'-' for k in FIELDS}|{'format':'NEVERGONE.GhidraClassOwnerAliasFlow/1','row_type':row_type,'class_name':cls,'function_entry':'00001000','function_name':cls+'::f','owner_parameter_proven':owner,'owner_parameter_type':cls+' *' if owner=='1' else 'CCPoint','owner_parameter_name':'this' if owner=='1' else 'p','store_count':'0','call_count':'0','owner_address_arg_count':'0','owner_field_value_arg_count':'0','owner_derived_unknown_arg_count':'0','owner_derived_unknown_store_count':'0'}
def call(cls,kind,off='-',owner='1',target='cocos2d::CCSprite::setOpacity'):
    r=base(cls,'call_arg',owner);r.update(op_address='00001010',op_time='1',call_kind='CALL',call_target_entry='00500000',call_target_name=target,arg_index='0',arg_token='x',arg_flow_kind=kind,arg_owner_offset=off);return r

def main():
    with tempfile.TemporaryDirectory() as td:
        d=Path(td);b=d/'b.tsv';j=d/'j.tsv';p=d/'p.tsv';out=d/'out.jsonl'
        wt(b,[base('BeatenEffect'),call('BeatenEffect','owner_field_value','0x128'),base('BeatenEffect','summary','0'),call('BeatenEffect','owner_address','0x0','0')])
        wt(j,[base('CCJoyStick'),call('CCJoyStick','owner_address','0x10c')])
        wt(p,[base('PVEDATA::equipToolsListMsg')])
        subprocess.run([sys.executable,str(AUD),'--class-flow','BeatenEffect',str(b),'--class-flow','CCJoyStick',str(j),'--class-flow','PVEDATA::equipToolsListMsg',str(p),'--details-output',str(out)],check=True,capture_output=True,text=True)
        rows=[json.loads(x) for x in out.read_text().splitlines() if x.strip()];by={x['class_name']:x for x in rows}
        assert by['BeatenEffect']['pointer_slot_alias_stability_proven'] is True
        assert len(by['BeatenEffect']['owner_field_value_calls'])==1
        assert by['BeatenEffect']['non_owner_parameter_methods']==1
        assert by['CCJoyStick']['pointer_slot_alias_stability_proven'] is False
        assert 'target_field_address_escape' in by['CCJoyStick']['reasons']
        assert by['PVEDATA::equipToolsListMsg']['pointer_slot_alias_stability_proven'] is True
    print('owner alias flow v23 smoke: ok')
if __name__=='__main__':main()
