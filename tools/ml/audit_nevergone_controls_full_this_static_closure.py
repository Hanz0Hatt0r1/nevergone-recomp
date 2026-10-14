#!/usr/bin/env python3
"""Close the exact original APK's 14 external unadjusted-Controls* first-hop sites.

This reconciles the old Ghidra frontier with later exact-native scheduler,
action, ControlsButton, parent/touch and lifetime-teardown evidence. It does
not claim whole-program alias exclusivity and never changes routing.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

from audit_nevergone_controls_retained_alias_closure import (
    OriginalElf, immediate_stores, require, verify_controlsbutton_callback_path,
)
from audit_nevergone_controls_scheduler_action_callbacks import (
    verify_scheduler_path, verify_callfuncn_path,
)
from audit_nevergone_controls_cocos_backrefs import Elf as CocosElf, parent, touch
from audit_nevergone_controls_parent_script_static_closure import (
    verify_parent_targets, verify_script_static_reachability,
)

FORMAT='NEVERGONE.ControlsFullThisStaticClosure/1'
SHA='94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e'
DANGEROUS={0x124,0x12c,0x130,0x134,0x174}
DIRECT={
 0x296728:('_ZN7cocos2d6CCNode22unscheduleAllSelectorsEv',0x516fc4,'lifetime_scheduler_cleanup'),
 0x29672e:('_ZN7cocos2d7CCLayerD2Ev',0x5253f8,'base_destructor'),
 0x29674e:('_ZdlPv',0x6fea70,'terminal_delete'),
 0x296978:('_ZN7cocos2d7CCLayerC2Ev',0x525204,'base_constructor'),
 0x297746:('_ZN7cocos2d6CCNode12scheduleOnceEMNS_8CCObjectEFvfEf',0x516f80,'scheduler_retained_callback'),
 0x297786:('_ZN7cocos2d6CCNode12scheduleOnceEMNS_8CCObjectEFvfEf',0x516f80,'scheduler_retained_callback'),
 0x2979c2:('_ZN7cocos2d11CCCallFuncN6createEPNS_8CCObjectEMS1_FvPNS_6CCNodeEE',0x50f42e,'action_retained_callback'),
 0x298118:('_ZN14ControlsButton6createEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE',0x2985fc,'button_retained_callback'),
 0x29816a:('_ZN14ControlsButton6createEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE',0x2985fc,'button_retained_callback'),
 0x2981bc:('_ZN14ControlsButton6createEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE',0x2985fc,'button_retained_callback'),
 0x2982b2:('_ZN7cocos2d6CCNode8scheduleEMNS_8CCObjectEFvfE',0x516f44,'scheduler_retained_callback'),
}
INDIRECT={
 0x2979e6:(0xb8,'_ZN7cocos2d6CCNode8addChildEPS0_',0x515dbc,'parent_backref'),
 0x297e74:(0x1ac,'_ZN7cocos2d7CCLayer15setTouchEnabledEb',0x524da8,'touch_delegate_backref'),
 0x297ef2:(0xb8,'_ZN7cocos2d6CCNode8addChildEPS0_',0x515dbc,'parent_backref'),
}
BASE_HASHES={
 '_ZN7cocos2d7CCLayerC2Ev':(0x525204,116,'3f2e3e01249857ae7e4d3cf895d7368b74e33793b9df533de75533a3d0bf6fb9'),
 '_ZN7cocos2d7CCLayerD2Ev':(0x5253f8,88,'7575bb07b4c3cb1bf510a3be2915b28629863f4e02d0299b02c9e6159bb1f54c'),
 '_ZN7cocos2d6CCNode22unscheduleAllSelectorsEv':(0x516fc4,10,'3f7ba58201f8b83e09be3cb691288a7ce4d69d4ff6a7d45b97cf09f90cf04b2c'),
 '_ZN7cocos2d11CCScheduler22unscheduleAllForTargetEPNS_8CCObjectE':(0x509020,264,'39264cd2af48c29ed5c3f7bffe1a7e3655fd895c63c78d3110c247a9732adf5b'),
 '_ZdlPv':(0x6fea70,8,'1724f670851a53b01c1cbddd2771f068e80dc96bf5b3af5fb040d4c442033ede'),
}


def bl_target(pc:int,a:int,b:int)->int|None:
 if (a&0xf800)!=0xf000 or (b&0xd000)!=0xd000:return None
 s=(a>>10)&1;j1=(b>>13)&1;j2=(b>>11)&1;i1=1^(j1^s);i2=1^(j2^s)
 imm=(s<<24)|(i1<<23)|(i2<<22)|((a&0x3ff)<<12)|((b&0x7ff)<<1)
 if s:imm-=1<<25
 return (pc+4+imm)&0xffffffff


def direct_target(elf:OriginalElf,site:int)->int:
 a,b=struct.unpack('<HH',elf.at(site,4)); t=bl_target(site,a,b); require(t is not None,'direct call encoding changed '+hex(site)); return t&~1


def verify_direct_sites(elf:OriginalElf)->list[dict]:
 rows=[]
 for site,(symbol,target,closure) in sorted(DIRECT.items()):
  value,_=elf.symbol(symbol,target)
  observed=direct_target(elf,site)
  require(observed==(value&~1)==target,'direct frontier target changed '+hex(site))
  rows.append({'site':f'0x{site:08x}','call_kind':'CALL','symbol':symbol,'target':f'0x{target:08x}','closure':closure})
 return rows


def verify_indirect_sites(elf:OriginalElf)->list[dict]:
 vt,size=elf.symbol('_ZTV8Controls',0x8e4490,592); require(size==592,'Controls vtable changed')
 rows=[]
 for site,(slot,symbol,target,closure) in sorted(INDIRECT.items()):
  insn=elf.at(site,2).hex(); require(insn in {'9847','9047'},'CALLIND BLX changed '+hex(site))
  entry=(vt&~1)+8+slot; require(elf.relative_relocations.get(entry)==23,'vtable relocation changed')
  value,_=elf.symbol(symbol,target); require(elf.u32(entry)==value,'vtable target changed '+hex(slot))
  rows.append({'site':f'0x{site:08x}','call_kind':'CALLIND','slot':hex(slot),'symbol':symbol,'target':f'0x{target:08x}','closure':closure})
 return rows


def verify_base_lifetime_calls(elf:OriginalElf)->dict:
 store_map={}
 for name,(address,size,digest) in BASE_HASHES.items():
  elf.symbol(name,address,size)
  require(hashlib.sha256(elf.at(address,size)).hexdigest()==digest,'base/lifetime function changed '+name)
  stores=immediate_stores(elf,address,size)
  dangerous=[row for row in stores if row[4] in DANGEROUS]
  require(not dangerous,'base/lifetime callee directly writes dangerous field '+name)
  store_map[name]=[hex(row[4]) for row in stores]
 # unscheduleAllSelectors moves this to r1, loads scheduler from this+0xc4 and
 # tail-calls CCScheduler::unscheduleAllForTarget. The scheduler's only wide
 # immediate STORE is its own +0x30 state byte; target is kept in r7.
 require(elf.at(0x516fc4,10).hex()=='0146d0f8c400f2f729b8','unscheduleAllSelectors forwarding changed')
 require(elf.at(0x509024,8).hex()=='054601910f462900','scheduler target preservation changed')
 require(elf.at(0x5090ec,4).hex()=='85f83030','scheduler internal +0x30 store changed')
 # Controls D0 invokes Controls D2 first, then operator delete. Any destructor
 # writes are terminal and cannot affect a later live receiver callsite.
 require(hashlib.sha256(elf.at(0x296744,16)).hexdigest()=='61ca0d4020bc1ef8bda96c96de2650e20292640ea12c47fb99cd53d012b1993b','Controls deleting destructor window changed')
 return {'direct_store_offsets':store_map,'dangerous_field_write':False,'delete_is_post_destructor_terminal':True,'unschedule_target_is_read_only_controls_argument':True}


def analyze(apk_path:Path)->dict:
 require(zipfile.is_zipfile(apk_path),'exact APK required')
 with zipfile.ZipFile(apk_path) as apk:
  data=apk.read('lib/armeabi-v7a/libcocos2dcpp.so')
  require(hashlib.sha256(data).hexdigest()==SHA,'ELF SHA mismatch')
  elf=OriginalElf(data); direct=verify_direct_sites(elf); indirect=verify_indirect_sites(elf); base=verify_base_lifetime_calls(elf)
  # Re-run the exact merged retained-channel proofs on the same original ELF.
  button=verify_controlsbutton_callback_path(elf)
  scheduler=verify_scheduler_path(elf)
  action=verify_callfuncn_path(elf)
  ce=CocosElf(data); parent_result=parent(ce); touch_result=touch(ce); parent_targets=verify_parent_targets(ce); script=verify_script_static_reachability(ce,apk)
 require(len(direct)==11 and len(indirect)==3,'frontier site count changed')
 return {'format':FORMAT,'elf_sha256':SHA,'external_full_this_sites':14,'direct_sites':direct,'indirect_sites':indirect,'base_lifetime_closure':base,'controlsbutton_callback_closure':button,'scheduler_closure':scheduler,'callfuncn_closure':action,'parent_backref_closure':{'parent':parent_result,'targets':parent_targets},'touch_backref_closure':{'touch':touch_result,'script_static':script},'external_full_this_static_frontier_closed':True,'known_frontier_sites_write_controls_pointer_fields':False,'whole_program_alias_writes_excluded':False,'receiver_class_proven':False,'affects_routing':False}


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('original_apk',type=Path);p.add_argument('--details-output',type=Path,required=True);a=p.parse_args();r=analyze(a.original_apk);a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text(json.dumps(r,sort_keys=True)+'\n');print('format:',FORMAT);print('external full-this sites:',r['external_full_this_sites']);print('static frontier closed: yes');print('known frontier sites write Controls pointer fields: no');print('whole-program alias exclusion: no; affects routing: no');print('details:',a.details_output)
if __name__=='__main__':main()
