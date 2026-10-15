#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,hashlib,io,json,struct,tarfile,zipfile
from pathlib import Path
from audit_nevergone_controlsbutton_owner_first_hop import Elf,SHA,req,read_member,tsv_bytes,TARGET_FIELDS

FORMAT='NEVERGONE.ControlsButtonOwnerFieldPackagedStability/1'
FIRST='NEVERGONE.ControlsButtonOwnerFieldFirstHop/1'
PARENT_SCRIPT='NEVERGONE.ControlsParentScriptStaticClosure/1'
DLSYM='NEVERGONE.ControlsDlsymClosure/1'

TARGET_WRITES={
 '002a8494':'0x104','002a849a':'0x108','002a84a8':'0x10c',
 '002a84cc':'0x104','002a84ea':'0x108','002a8510':'0x10c',
 '002a8596':'0x104','002a859e':'0x108','002a85a4':'0x10c',
 '002a85c4':'0x104','002a85de':'0x108',
}
NONZERO_CALLS={
 ('002a8406','002a8426','0xe0','address:0055943e:8'),
 ('002a842a','002a8444','0xe0','address:005595d8:8'),
 ('002a8448','002a8478','0x120','address:0052782c:8'),
 ('002a86b8','002a86c6','0x120','address:0052785a:8'),
 ('002a86b8','002a871c','0x120','address:00527a3a:8'),
 ('002a87d2','002a8808','0x120','address:0052785a:8'),
 ('002a87d2','002a883a','0x120','address:00527a3a:8'),
 ('002a88e2','002a8926','0x120','address:0052785a:8'),
 ('002a88e2','002a8980','0x120','address:00527a3a:8'),
}
FUNCTION_HASHES={
 '_ZN7cocos2d7CCPointC2Ev':(0x51782c,16,'558ede2ccbf4262696ee979c32ce0f1fa3623218eb9e4030da396e12c45ff9c7'),
 '_ZN7cocos2d7CCPointaSERKS0_':(0x51785a,18,'cd02f0c22aaa712f8942d683e2f325fdf67e467f68febb8ca78f6dcf8934ce5e'),
 '_ZNK7cocos2d6CCRect13containsPointERKNS_7CCPointE':(0x517a3a,116,'c2725ce33cb69f2844adf06069cf84a257f80bd3cc5c50401d9ed6c07c276aca'),
 '_ZN14ControlsButton7onEnterEv':(0x298406,36,'84092c2f0e00811d3c4d6ef6ea24a81273e5dc7cc2abf1ba29677c3e35bead17'),
 '_ZN14ControlsButton6onExitEv':(0x29842a,30,'07dc299fafcf2b5a9c7891e2f6d997a7efc49cb5779e5363a07724fade34eeb9'),
 '_ZN7cocos2d17CCTouchDispatcher19addTargetedDelegateEPNS_15CCTouchDelegateEib':(0x54943e,76,'441a9dcaf7fa6a3f9661ea52dafe7512560c72da09e5c3db9609ba914553a530'),
 '_ZN7cocos2d17CCTouchDispatcher14removeDelegateEPNS_15CCTouchDelegateE':(0x5495d8,62,'6a4c967618b344e8e03f29ca095e7107ae0b7493885f178c67343b6d81b87bd2'),
 '_ZN7cocos2d22CCTargetedTouchHandler19handlerWithDelegateEPNS_15CCTouchDelegateEib':(0x549de8,84,'00ad32e7aaf65f8161330b47f3483ae51134a16b11a25f868e8c2c5e5b1093b2'),
 '_ZN7cocos2d14CCTouchHandler16initWithDelegateEPNS_15CCTouchDelegateEi':(0x549bc4,56,'2c9d8d8f0b8377b603167b66f45adc68a68c4eb72ab9227ab85630a19da4d989'),
 '_ZN7cocos2d17CCTouchDispatcher7touchesEPNS_5CCSetEPNS_7CCEventEj':(0x549618,636,'d14d1ad40cf51e6fb2cf3aafba6c223aeb0a90dcd9e280e113b4a6a2a1a67a2c'),
 '_ZN7cocos2d7CCLayerC2Ev':(0x525204,116,'3f2e3e01249857ae7e4d3cf895d7368b74e33793b9df533de75533a3d0bf6fb9'),
 '_ZN7cocos2d7CCLayerD2Ev':(0x5253f8,88,'7575bb07b4c3cb1bf510a3be2915b28629863f4e02d0299b02c9e6159bb1f54c'),
 '_ZN7cocos2d7CCLayer7onEnterEv':(0x525b2a,82,'82c1979e58f12d8d73dd8f3b4462f54d40e05539a6cb714c8f74f88eb3092a08'),
 '_ZN7cocos2d7CCLayer6onExitEv':(0x525b7c,82,'d5011a403a11a44ea202a019a083724003904649659a27c1208daa58dc7ad60e'),
 '_ZN7cocos2d6CCNode9getParentEv':(0x515d46,6,'91a7a13cf1190052522b521426e4d512bb2c17cd4bc3b4708eb961838cd14b27'),
 '_ZN7cocos2d6CCNode11removeChildEPS0_b':(0x516c1e,36,'23274966faa32352b2b3b5792e65cc0182b38084c80c7981319f6705908471fa'),
 '_ZN7cocos2d6CCNode12reorderChildEPS0_i':(0x515e20,52,'630393dddb2bdc5563c9829e56cad1d8235b09e809226b63c0426db95e0e8ff1'),
 '_ZN7cocos2d6CCNode21nodeToParentTransformEv':(0x516200,600,'a4d7fb38ffaa9deb036430df007f682f79d6a0f59b7180a7e99b8f4f96372e17'),
 '_ZN7cocos2d6CCNode9transformEv':(0x516cc8,134,'2ab32f86809750ba2e251ce30ffe2f01092db8d7b06eaef96231abc9d2e161fd'),
}
PARENT_ACCESSES={0x515c8c:'load',0x515d46:'load',0x515d4c:'store',0x515de2:'load',0x5161bc:'load',0x5168ee:'store',0x5169e2:'store',0x516e10:'load',0x516e1a:'load',0x539354:'load',0x5399ce:'load',0x539a46:'load'}
PARENT_SLOTS={
 0xcc:'_ZN7cocos2d6CCNode9getParentEv',
 0xdc:'_ZN7cocos2d6CCNode11removeChildEPS0_b',
 0xf0:'_ZN7cocos2d6CCNode12reorderChildEPS0_i',
 0x160:'_ZN7cocos2d6CCNode21nodeToParentTransformEv',
}
PARENT_STORES={
 '_ZN7cocos2d6CCNode9getParentEv':[],
 '_ZN7cocos2d6CCNode11removeChildEPS0_b':[],
 '_ZN7cocos2d6CCNode12reorderChildEPS0_i':[(0x515e24,0xd2,1)],
 '_ZN7cocos2d6CCNode21nodeToParentTransformEv':[(0x51641a,0xcf,1),(0x516420,0xcd,1)],
 '_ZN7cocos2d6CCNode9transformEv':[],
}
SECONDARY={
 0x0:'_ZThn224_N14ControlsButtonD1Ev',
 0x4:'_ZThn224_N14ControlsButtonD0Ev',
 0x8:'_ZThn224_N14ControlsButton12ccTouchBeganEPN7cocos2d7CCTouchEPNS0_7CCEventE',
 0xc:'_ZThn224_N14ControlsButton12ccTouchMovedEPN7cocos2d7CCTouchEPNS0_7CCEventE',
 0x10:'_ZThn224_N14ControlsButton12ccTouchEndedEPN7cocos2d7CCTouchEPNS0_7CCEventE',
 0x14:'_ZThn224_N7cocos2d7CCLayer16ccTouchCancelledEPNS_7CCTouchEPNS_7CCEventE',
}

def j(path): return json.loads(Path(path).read_text().strip())

def validate_prerequisites(first, ps, ds):
 req(first.get('format')==FIRST and first.get('status')=='controlsbutton_owner_first_hop_classified' and first.get('target_fields_written_by_first_hop_external_targets') is False,'first-hop proof invalid')
 req(ps.get('format')==PARENT_SCRIPT and ps.get('known_parent_native_dangerous_write_through_excluded') is True and ps.get('packaged_static_script_engine_write_through_reachable') is False,'parent/script global proof invalid')
 req(ds.get('format')==DLSYM and ds.get('packaged_dlsym_lookup_of_set_script_engine_excluded') is True and ds.get('packaged_cross_library_lookup_of_set_script_engine_excluded') is True,'dlsym global proof invalid')

def immediate_stores(e,a,z):
 out=[]
 for pc in range(a,a+z-3,2):
  x,y=struct.unpack('<HH',e.at(pc,4));op=x&0xfff0
  if op in (0xf8c0,0xf880): out.append((pc,y&0xfff,4 if op==0xf8c0 else 1))
 return out

def verify_hashes(e):
 for n,(a,s,h) in FUNCTION_HASHES.items():
  v,z,_,_=e.sym(n);req((v&~1)==a and z==s,'symbol identity changed '+n);req(hashlib.sha256(e.at(a,s)).hexdigest()==h,'function changed '+n)

def verify_owner(archive):
 with tarfile.open(archive,'r:gz') as tf:
  audit=json.loads(read_member(tf,'controlsbutton-owner-field-alias-audit.jsonl').decode().strip())
  rows=tsv_bytes(read_member(tf,'evidence/controlsbutton-owner-field-aliases.tsv'))
 req(audit['direct_target_field_writes']==TARGET_WRITES,'target field write inventory changed')
 req(audit['parameter_zero_argument_offsets']=={'0x0':23,'0x120':7,'0xe0':2},'owner call offset split changed')
 target_rows=[r for r in rows if r['row_type']=='store' and r['address_parameter_index']=='0' and r['address_offset'] in {'0x104','0x108','0x10c'}]
 req({r['op_address']:r['address_offset'] for r in target_rows}==TARGET_WRITES,'target store rows changed')
 forbidden_flags={'0xec','0xed','0xee','0xf0','0xfc','0x100'}
 req(not [r for r in rows if r['row_type']=='store' and r['address_parameter_index']=='0' and r['address_offset'] in forbidden_flags],'ControlsButton changes CCLayer activation flags')
 nonzero=set()
 for r in rows:
  if r['row_type']!='call':continue
  offs=[] if r['param0_arg_offsets']=='-' else r['param0_arg_offsets'].split(',')
  if offs and any(x!='0x0' for x in offs):
   for x in offs:
    if x!='0x0': nonzero.add((r['function_entry'],r['op_address'],x,r['call_target_token']))
 req(nonzero==NONZERO_CALLS,'nonzero owner call surface changed')
 return rows

def verify_ccpoint(e):
 req(e.at(0x51782c,16).hex()=='002110b50a460446fff7f7ff204610bd','CCPoint ctor body changed')
 req(e.at(0x51785a,18).hex()=='0b4610b5044609685a68fff7dfff204610bd','CCPoint assignment body changed')
 return {'subobject_offset':'0x120','call_count':7,'targets':['CCPoint::CCPoint','CCPoint::operator=','CCRect::containsPoint'],'can_recover_owner':False}

def verify_touch(e):
 req(e.at(0x298418,4).hex()=='04f1e001' and e.at(0x29843c,4).hex()=='04f1e001','touch delegate adjustment changed')
 req(e.at(0x549bc8,2).hex()=='4161','touch handler delegate store changed')
 req(e.at(0x549be4,4).hex()=='cff775fe','touch handler delegate retain changed')
 base=0x8e48dc
 resolved={}
 for off,n in SECONDARY.items():
  raw,_,_,_=e.sym(n);req(e.rels.get(base+off)==23 and e.u32(base+off)==raw,'ControlsButton touch secondary vtable changed '+hex(off));resolved[hex(off)]=n
 return {'retained_delegate':'ControlsButton+0xe0','handler_field':'CCTouchHandler+0x14','secondary_vtable_address_point':'0x008e48dc','secondary_entries':resolved,'callback_methods_in_19_method_inventory':True,'target_fields_written_by_touch_callbacks_or_first_hop':False}

def verify_parent(e):
 pref=('_ZN7cocos2d6CCNode','_ZNK7cocos2d6CCNode','_ZN7cocos2d7CCLayer','_ZNK7cocos2d7CCLayer','_ZN7cocos2d8CCSprite','_ZNK7cocos2d8CCSprite')
 obs={};seen=set()
 for n,(v,s,t,sh) in e.symbols.items():
  a=v&~1
  if t!=2 or not s or not n.startswith(pref) or (a,s) in seen:continue
  seen.add((a,s))
  for pc in range(a,a+s-3,2):
   x,y=struct.unpack('<HH',e.at(pc,4));op=x&0xfff0
   if (y&0xfff)==0xa8 and op in (0xf8d0,0xf8c0):obs[pc]='load' if op==0xf8d0 else 'store'
 req(obs==PARENT_ACCESSES,'CCNode parent access inventory changed')
 vraw,vsz,_,_=e.sym('_ZTV14ControlsButton');req((vraw&~1)==0x8e46f0 and vsz==560,'ControlsButton vtable changed');vptr=0x8e46f8
 slots={}
 for slot,n in PARENT_SLOTS.items():
  raw,_,_,_=e.sym(n);req(e.rels.get(vptr+slot)==23 and e.u32(vptr+slot)==raw,'ControlsButton parent slot changed '+hex(slot));slots[hex(slot)]=n
 for n,expected in PARENT_STORES.items():
  a,s,_=FUNCTION_HASHES[n];actual=immediate_stores(e,a,s);req(actual==expected,'parent target STORE inventory changed '+n);req(not ({o for _,o,_ in actual}&TARGET_FIELDS),'parent target writes target ControlsButton field '+n)
 req(e.at(0x516e10,22).hex()=='d0f8a80038b1fff7f9ffd4f8a800bde81040fff751bf','transformAncestors parent path changed')
 return {'parent_field':'child CCNode+0xa8','parent_access_count':len(obs),'controlsbutton_parent_slots':slots,'parent_targets_write_sprite_fields':False}

def verify_lifecycle(e):
 for n in ['_ZN7cocos2d7CCLayerC2Ev','_ZN7cocos2d7CCLayerD2Ev','_ZN7cocos2d7CCLayer7onEnterEv','_ZN7cocos2d7CCLayer6onExitEv']:
  a,s,_=FUNCTION_HASHES[n];req(not ({o for _,o,_ in immediate_stores(e,a,s)}&TARGET_FIELDS),'CCLayer lifecycle writes target field '+n)
 return {'base':'cocos2d::CCLayer','activation_flags_rewritten_by_controlsbutton':False,'lifecycle_directly_writes_sprite_fields':False}

def main():
 p=argparse.ArgumentParser();p.add_argument('apk',type=Path);p.add_argument('owner_archive',type=Path);p.add_argument('first_hop_json',type=Path);p.add_argument('parent_script_json',type=Path);p.add_argument('dlsym_json',type=Path);p.add_argument('--details-output',required=True,type=Path);a=p.parse_args()
 with zipfile.ZipFile(a.apk) as z:e=Elf(z.read('lib/armeabi-v7a/libcocos2dcpp.so'))
 verify_hashes(e);verify_owner(a.owner_archive)
 first=j(a.first_hop_json);ps=j(a.parent_script_json);ds=j(a.dlsym_json);validate_prerequisites(first,ps,ds)
 ccpoint=verify_ccpoint(e);touch=verify_touch(e);parent=verify_parent(e);life=verify_lifecycle(e)
 r={'format':FORMAT,'scope':'exact_packaged_apk_normal_code_execution','elf_sha256':SHA,'target_fields':['0x104','0x108','0x10c'],'first_hop_closed':True,'ccpoint_subobject_channel':ccpoint,'targeted_touch_channel':touch,'parent_backref_channel':parent,'cclayer_lifecycle_channel':life,'packaged_script_engine_write_through_reachable':False,'packaged_dlsym_setter_lookup_excluded':True,'owner_fields_stable':True,'receiver_class_proven':False,'whole_program_alias_writes_excluded':False,'external_injected_code_excluded':False,'affects_routing':False}
 a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text(json.dumps(r,sort_keys=True)+'\n')
 print('format:',FORMAT);print('scope:',r['scope']);print('target fields stable in packaged program: 3/3');print('CCPoint+0x120 subobject calls closed: 7/7');print('targeted-touch retained delegate channel closed: yes');print('child parent back-reference channel closed: yes');print('CCLayer lifecycle/script packaged channel closed: yes');print('owner fields stable: proven');print('whole-program alias exclusion: no; affects routing: no');print('details:',a.details_output)
if __name__=='__main__':main()
