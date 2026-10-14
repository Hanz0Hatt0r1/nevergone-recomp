#!/usr/bin/env python3
from __future__ import annotations
import copy
from audit_nevergone_controlsbutton_owner_field_probe import audit,EXPECTED,GH,PLAN

def fixture():
 entries=[f'{0x2a83b0+i*0x10:08x}' for i in range(19)]
 plan=[{'format':PLAN,'controlsbutton_symbol_count':21,'controlsbutton_unique_method_count':19,'controlsbutton_method_entries':entries,'direct_target_field_store_count':11}]
 manifest=[{'function_entry':e} for e in entries]
 rows=[]
 for i,e in enumerate(entries):
  rows.append({'format':GH,'row_type':'summary','function_entry':e,'decompile_status':'ok','store_count':'11' if i==0 else '0','call_count':'1' if i==0 else '0','unknown_store_count':'0','param0_call_count':'1' if i==0 else '0','op_address':'-','op_time':'-1'})
 for addr,field in EXPECTED.items():
  rows.append({'format':GH,'row_type':'store','function_entry':entries[0],'decompile_status':'ok','op_address':addr,'op_time':'0','address_parameter_index':'0','address_offset':field,'address_kind':'base_plus_constant','address_root_token':'register:register:00000000:4'})
 rows.append({'format':GH,'row_type':'call','function_entry':entries[0],'decompile_status':'ok','op_address':'002a9000','op_time':'1','param0_arg_indices':'0','param0_arg_offsets':'0x0','unknown_arg_indices':'-','call_kind':'CALL','call_target_token':'address:00500000:8'})
 return plan,manifest,rows

def incomplete(plan,manifest,rows,needle):
 r=audit(plan,manifest,rows);assert r['status']=='bounded_controlsbutton_owner_probe_incomplete',r;assert any(needle in x for x in r['reasons']),r

def main():
 p,m,r=fixture();x=audit(p,m,r);assert x['status']=='bounded_controlsbutton_owner_probe_complete';assert x['direct_target_field_writes']==EXPECTED;assert x['all_stores']==11 and x['all_calls']==1;assert x['parameter_zero_argument_offsets']=={'0x0':1};assert x['owner_fields_stable'] is False and x['affects_routing'] is False
 b=copy.deepcopy(r);b[0]['decompile_status']='decompile_failed';incomplete(p,m,b,'method_not_decompiled')
 b=copy.deepcopy(r);b[0]['store_count']='10';incomplete(p,m,b,'operation_count_mismatch')
 b=copy.deepcopy(r);b[19]['address_offset']='0x110';incomplete(p,m,b,'direct_target_field_writer_set_changed')
 b=copy.deepcopy(r);b[19]['address_parameter_index']='-';b[0]['unknown_store_count']='1';incomplete(p,m,b,'direct_target_field_writer_set_changed')
 try:audit(p,m[:-1],r)
 except ValueError:pass
 else:raise AssertionError('incomplete manifest accepted')
 print('ControlsButton owner field probe smoke: ok')
if __name__=='__main__':main()
