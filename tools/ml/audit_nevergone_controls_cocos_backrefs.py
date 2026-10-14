#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, struct, zipfile
from pathlib import Path

SHA='94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e'
FORMAT='NEVERGONE.ControlsCocosBackrefs/1'
DANGEROUS={0x124,0x12c,0x130,0x134,0x174}
FUNCTION_HASHES={
'_ZN7cocos2d6CCNode8addChildEPS0_':(0x515dbc,20,'a0eb9ad6ac2dc6a637de8fbf4952c664b260ba389a843325e5c753c09ef48c05'),
'_ZN7cocos2d6CCNode8addChildEPS0_ii':(0x516c64,100,'3f521af5ee27d63b7aaa0317f481e39e0dbcd7e77040fdf5cf70b8f4b6f12ee5'),
'_ZN7cocos2d6CCNode9setParentEPS0_':(0x515d4c,6,'7d2fab5043c2f13a4694e84cd69bfd258ee035dde971f352183a03dfe2fa559e'),
'_ZN7cocos2d7CCLayer15setTouchEnabledEb':(0x524da8,58,'f956011b464506018389df0ebcc76eaa246d2801f331871853db116e6c58144c'),
'_ZN7cocos2d7CCLayer27registerWithTouchDispatcherEv':(0x5248f2,68,'9e54c7f79edf22d7d6756c2e9a9c888802d13523a3aa626b72340ba99af47ec2'),
'_ZN7cocos2d17CCTouchDispatcher19addStandardDelegateEPNS_15CCTouchDelegateEi':(0x5493f4,74,'38eb3a1e6a2b11b2bd2a64d34985b396cf62158c8abadb85601665c6a2c1970d'),
'_ZN7cocos2d22CCStandardTouchHandler19handlerWithDelegateEPNS_15CCTouchDelegateEi':(0x549d68,76,'14062632e397da576626865d457366bc24296d4fffbac3c232ea504b5347e24c'),
'_ZN7cocos2d14CCTouchHandler16initWithDelegateEPNS_15CCTouchDelegateEi':(0x549bc4,56,'2c9d8d8f0b8377b603167b66f45adc68a68c4eb72ab9227ab85630a19da4d989'),
'_ZN7cocos2d7CCLayer7onEnterEv':(0x525b2a,82,'82c1979e58f12d8d73dd8f3b4462f54d40e05539a6cb714c8f74f88eb3092a08'),
'_ZN7cocos2d6CCNodeC2Ev':(0x516970,248,'c9c4a1128cd8b34cb899e9c6e10743e0eac4002c1a9d17c0e8323c3a79d4cd66'),
'_ZN7cocos2d8CCSprite6createEPKc':(0x539614,58,'298db55b0a638fceea6a0204a61f0c53f808fce4142a96d641cb63e4e873c4f2'),
'_ZN7cocos2d7CCLayer6createEv':(0x525278,52,'cd972db0ebd410b9f4fd4491cd0a0d3638f0303086b6ec39de4a6d2288f1410f'),
'_ZN7cocos2d7CCLayer14ccTouchesBeganEPNS_5CCSetEPNS_7CCEventE':(0x525350,16,'ddee523e11cefb081ba64dd913fdc85f4154d3dfd87de920b8e87d6d5bce935f'),
'_ZN7cocos2d7CCLayer14ccTouchesMovedEPNS_5CCSetEPNS_7CCEventE':(0x525368,16,'5060d1104bc183162aa6d10a74d722f09b324ef0198316e75fb2c94528d65a3a'),
'_ZN7cocos2d7CCLayer14ccTouchesEndedEPNS_5CCSetEPNS_7CCEventE':(0x525380,16,'e47d1eaa5e65c7ce13303d08e7f522237a921dfa3f4d94f85fbb7ed6f6202694'),
'_ZN7cocos2d7CCLayer18ccTouchesCancelledEPNS_5CCSetEPNS_7CCEventE':(0x525398,16,'6d8a7a2883cc201e015a45cef3c0a33f401c30ce3ee432076574f3e8f42c93ef'),
}
PARENT_ACCESSES={0x515c8c:'load',0x515d46:'load',0x515d4c:'store',0x515de2:'load',0x5161bc:'load',0x5168ee:'store',0x5169e2:'store',0x516e10:'load',0x516e1a:'load',0x539354:'load',0x5399ce:'load',0x539a46:'load'}
TOUCH_THUNKS={
0x200:'_ZThn224_N7cocos2d7CCLayer12ccTouchBeganEPNS_7CCTouchEPNS_7CCEventE',
0x204:'_ZThn224_N7cocos2d7CCLayer12ccTouchMovedEPNS_7CCTouchEPNS_7CCEventE',
0x208:'_ZThn224_N7cocos2d7CCLayer12ccTouchEndedEPNS_7CCTouchEPNS_7CCEventE',
0x20c:'_ZThn224_N7cocos2d7CCLayer16ccTouchCancelledEPNS_7CCTouchEPNS_7CCEventE',
0x210:'_ZThn224_N7cocos2d7CCLayer14ccTouchesBeganEPNS_5CCSetEPNS_7CCEventE',
0x214:'_ZThn224_N7cocos2d7CCLayer14ccTouchesMovedEPNS_5CCSetEPNS_7CCEventE',
0x218:'_ZThn224_N7cocos2d7CCLayer14ccTouchesEndedEPNS_5CCSetEPNS_7CCEventE',
0x21c:'_ZThn224_N7cocos2d7CCLayer18ccTouchesCancelledEPNS_5CCSetEPNS_7CCEventE'}

def req(c,m):
 if not c: raise ValueError(m)

class Elf:
 def __init__(self,data:bytes):
  req(hashlib.sha256(data).hexdigest()==SHA,'ELF SHA mismatch'); req(data[:6]==b'\x7fELF\x01\x01','not ELF32 LE ARM')
  self.data=data; shoff=struct.unpack_from('<I',data,32)[0]; es,n,si=struct.unpack_from('<HHH',data,46); req(es==40 and 0<n<256 and si<n,'bad section table')
  self.sections=[struct.unpack_from('<IIIIIIIIII',data,shoff+i*es) for i in range(n)]
  shstr=self.sections[si]; names=data[shstr[4]:shstr[4]+shstr[5]]; self.byname={}
  for s in self.sections:
   e=names.find(b'\0',s[0]); req(e>=0,'bad section name'); self.byname[names[s[0]:e].decode()]=s
  ds=self.byname['.dynsym']; st=self.byname['.dynstr']; strings=data[st[4]:st[4]+st[5]]; self.symbols={}; self.functions=[]
  for p in range(ds[4],ds[4]+ds[5],16):
   no,v,z,info,_,sh=struct.unpack_from('<IIIBBH',data,p); e=strings.find(b'\0',no); req(e>=0,'bad symbol'); name=strings[no:e].decode('utf-8','replace'); typ=info&15
   if name:self.symbols[name]=(v,z,typ,sh)
   if typ==2 and v and z:self.functions.append((v&~1,z,name))
  self.rels={}
  for s in self.sections:
   if s[1]!=9: continue
   req(s[9]==8 and s[5]%8==0,'bad rel')
   for p in range(s[4],s[4]+s[5],8):
    a,i=struct.unpack_from('<II',data,p); self.rels[a]=i&255
 def at(self,a,n):
  m=[s for s in self.sections if s[1]==1 and s[3]<=a and a+n<=s[3]+s[5]]; req(len(m)==1,f'unmapped {a:x}'); s=m[0]; o=s[4]+a-s[3]; return self.data[o:o+n]
 def u32(self,a): return struct.unpack('<I',self.at(a,4))[0]
 def sym(self,n,a=None,z=None):
  req(n in self.symbols,'missing '+n); v,s,t,_=self.symbols[n]; req(t in (1,2),'bad type '+n)
  if a is not None:req((v&~1)==a,'addr '+n)
  if z is not None:req(s==z,'size '+n)
  return v,s

def func_hashes(e):
 for n,(a,z,h) in FUNCTION_HASHES.items(): e.sym(n,a,z); req(hashlib.sha256(e.at(a,z)).hexdigest()==h,'hash '+n)

def vslot(e,vt,va,vs,slot,target,ta):
 v,z=e.sym(vt,va,vs); entry=va+8+slot; req(e.rels.get(entry)==23,'rel '+vt); tv,_=e.sym(target,ta); req(e.u32(entry)==tv,'slot '+vt)

def parent_inventory(e):
 pref=('_ZN7cocos2d6CCNode','_ZNK7cocos2d6CCNode','_ZN7cocos2d7CCLayer','_ZNK7cocos2d7CCLayer','_ZN7cocos2d8CCSprite','_ZNK7cocos2d8CCSprite')
 obs={}; owners={}; seen=set()
 for st,sz,n in e.functions:
  if not n.startswith(pref) or (st,sz) in seen: continue
  seen.add((st,sz))
  for pc in range(st,st+sz-3,2):
   a,b=struct.unpack('<HH',e.at(pc,4)); op=a&0xfff0
   if (b&0xfff)==0xa8 and op in (0xf8d0,0xf8c0): obs[pc]='load' if op==0xf8d0 else 'store'; owners.setdefault(pc,set()).add(n)
 req(obs==PARENT_ACCESSES,'parent access inventory changed')
 return [{'address':f'0x{pc:08x}','access':k,'functions':sorted(owners.get(pc,set()))} for pc,k in sorted(obs.items())]

def parent(e):
 req(hashlib.sha256(e.at(0x297938,0xb0)).hexdigest()=='80c89265d8835cfd35370107a119018fee4be8603f55ec8d1e9014b10cc740c7','showAToJTip window')
 req(e.at(0x297ee0,20).hex()=='8df2caf922680146c4f878012046d2f8b8209047','init layer window')
 vslot(e,'_ZTVN7cocos2d8CCSpriteE',0x9263d0,632,0xc8,'_ZN7cocos2d6CCNode9setParentEPS0_',0x515d4c)
 vslot(e,'_ZTVN7cocos2d7CCLayerE',0x91cf50,560,0xc8,'_ZN7cocos2d6CCNode9setParentEPS0_',0x515d4c)
 req(e.at(0x515d4c,6).hex()=='c0f8a8107047','setParent bytes')
 return {'retained_children':[{'source':'Controls::showAToJTip','class':'cocos2d::CCSprite','parent_field':'CCNode+0xa8','controls_storage':'temporary child'},{'source':'Controls::init','class':'cocos2d::CCLayer','parent_field':'CCNode+0xa8','controls_storage':'Controls+0x178'}],'parent_field_direct_access_inventory':parent_inventory(e),'parent_backref_established':True,'parent_write_through_to_controls_pointer_fields_excluded':False}

def stores(e,a,z):
 r=[]
 for pc in range(a,a+z-3,2):
  x,y=struct.unpack('<HH',e.at(pc,4));
  if (x&0xfff0) in (0xf8c0,0xf880): r.append((pc,y&0xfff))
 return r

def touch(e):
 req(hashlib.sha256(e.at(0x29697c,0x30)).hexdigest()=='da69d8d680648e9409e4402d1b767fd30a61209c869472cb912f1de64cc14728','Controls vptr window')
 cv,sz=e.sym('_ZTV8Controls',0x8e4490,592); req(e.u32(0x296a0c)+0x296986==0x92ee04,'GOT calc'); req(e.rels.get(0x92ee04)==23 and e.u32(0x92ee04)==cv,'Controls GOT')
 vslot(e,'_ZTV8Controls',0x8e4490,592,0x19c,'_ZN7cocos2d7CCLayer27registerWithTouchDispatcherEv',0x5248f2)
 vslot(e,'_ZTV8Controls',0x8e4490,592,0x1ac,'_ZN7cocos2d7CCLayer15setTouchEnabledEb',0x524da8)
 req(hashlib.sha256(e.at(0x297e6a,12)).hexdigest()=='c6744de2bdbad52a749d698dd6d9fe2ccd69e18f6be306d19c9c39868af7c86f','setTouch call')
 req(hashlib.sha256(e.at(0x525246,20)).hexdigest()=='bbda5224f541c1f00fca3207b8663c96cf73f98acd5932959a38d0c37730a14b','touch defaults')
 forbidden={0xf0,0xfc,0x100}; writes=[]
 for st,sz,n in e.functions:
  if not (n.startswith('_ZN8Controls') or n.startswith('_ZNK8Controls')): continue
  for pc in range(st,st+sz-3,2):
   x,y=struct.unpack('<HH',e.at(pc,4))
   if (x&0xfff0) in (0xf8c0,0xf880) and (y&0xfff) in forbidden:writes.append((pc,y&0xfff,n))
 req(not writes,'Controls rewrites touch config')
 req(hashlib.sha256(e.at(0x525b32,18)).hexdigest()=='b221bccb9075fd57f103afaa253a696c4ef693bc7716dcb77a6c214802500e41','onEnter path')
 req(e.at(0x524904,4).hex()=='04f1e001','delegate +e0'); req(e.at(0x52491a,4).hex()=='d4f80021','mode +100'); req(e.at(0x524920,8).hex()=='bde8104024f066bd','standard delegate tail')
 req(e.at(0x549bc8,2).hex()=='4161','handler delegate +14'); req(e.at(0x549be4,4).hex()=='cff775fe','handler retain'); req(e.at(0x549d8e,2).hex()=='3146','standard factory forwards delegate')
 req(e.u32(0x8e4490+0x1f0)==0xffffff20,'touch this adjustment')
 for off,n in TOUCH_THUNKS.items():
  v,_=e.sym(n); entry=0x8e4490+off; req(e.rels.get(entry)==23 and e.u32(entry)==v,'touch thunk slot'); req(e.at(v&~1,4).hex()=='a0f1e000','touch thunk adjust')
 direct={}
 for n in ('_ZN7cocos2d7CCLayer14ccTouchesBeganEPNS_5CCSetEPNS_7CCEventE','_ZN7cocos2d7CCLayer14ccTouchesMovedEPNS_5CCSetEPNS_7CCEventE','_ZN7cocos2d7CCLayer14ccTouchesEndedEPNS_5CCSetEPNS_7CCEventE','_ZN7cocos2d7CCLayer18ccTouchesCancelledEPNS_5CCSetEPNS_7CCEventE'):
  a,z,_=FUNCTION_HASHES[n]; ss=stores(e,a,z); req(not [x for x in ss if x[1] in DANGEROUS],'touch callback dangerous write'); direct[n]=[hex(o) for _,o in ss]
 req(e.at(0x516a56,4).hex()=='c4f8dc00','script gate init'); req(e.at(0x525350,4).hex()=='d0f8dc30','script gate read')
 return {'delegate_subobject':'Controls+0xe0','retained_handler_field':'CCTouchHandler+0x14','registration_mode':'standard_delegate','controls_touch_vtable_inherits_cclayer':True,'inherited_touch_callback_direct_stores':direct,'script_engine_gate_field':'Controls+0xdc','native_touch_callbacks_write_controls_pointer_fields':False,'script_engine_write_through_excluded':False}

def analyze(data):
 e=Elf(data); func_hashes(e); p=parent(e); t=touch(e); return {'format':FORMAT,'elf_sha256':SHA,'parent_backref':p,'touch_dispatch_backref':t,'known_cocos_retained_channels_checked':2,'local_cocos_backrefs_characterized':True,'parent_write_through_to_controls_pointer_fields_excluded':False,'script_engine_write_through_excluded':False,'whole_program_alias_writes_excluded':False,'receiver_class_proven':False,'affects_routing':False}

def main():
 ap=argparse.ArgumentParser(); ap.add_argument('original_apk_or_elf',type=Path); ap.add_argument('--details-output',type=Path,required=True); a=ap.parse_args()
 if zipfile.is_zipfile(a.original_apk_or_elf):
  with zipfile.ZipFile(a.original_apk_or_elf) as z:data=z.read('lib/armeabi-v7a/libcocos2dcpp.so')
 else:data=a.original_apk_or_elf.read_bytes()
 r=analyze(data); a.details_output.parent.mkdir(parents=True,exist_ok=True); a.details_output.write_text(json.dumps(r,sort_keys=True)+'\n')
 print('format:',FORMAT); print('parent direct access sites:',len(r['parent_backref']['parent_field_direct_access_inventory'])); print('touch registration mode:',r['touch_dispatch_backref']['registration_mode']); print('retained touch delegate:',r['touch_dispatch_backref']['delegate_subobject'],'->',r['touch_dispatch_backref']['retained_handler_field']); print('open hazards: parent write-through, script-engine write-through'); print('whole-program alias exclusion: no; affects routing: no'); print('details:',a.details_output)
if __name__=='__main__': main()
