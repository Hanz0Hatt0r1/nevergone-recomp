#!/usr/bin/env python3
"""Bound every packaged libcocos2dcpp dlsym site relevant to Controls script setter.

The library imports dlsym for OpenSL, Android assets and getauxval. This audit
proves the exact direct dlsym PLT call surface and the only direct callers of
the two generic OpenSL helper wrappers. It excludes a packaged lookup of the
Cocos setScriptEngine symbol while remaining conservative about injected code.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

from audit_nevergone_controls_cocos_backrefs import Elf, SHA, req

FORMAT='NEVERGONE.ControlsDlsymClosure/1'
FFMPEG_SHA='f5e8392d27b182671be5070bc7aa5772a8b19711af222833651faf8516eacdde'
DLSYM_GOT=0x931eb8
EXPECTED_DLSYM_CALLS={0x54a7c6,0x54a806,0x54a924,0x54a938,0x54a950,0x54a966,0x635bd2}
GET_INTERFACE=0x54a7b4
GET_FUNC=0x54a7f4
EXPECTED_INTERFACE_CALLS={0x54aa04,0x54aa10,0x54aa18,0x54aa64,0x54aa7c,0x54aa92,0x54ac0a,0x54ac1c}
EXPECTED_FUNC_CALLS={0x54abe0}
FUNCTION_HASHES={
 GET_INTERFACE:(64,'a7ca92b8eb149f8333c36aac98dde92dc5fd0f737b1bf7fbb9aa75f7461f6f55'),
 GET_FUNC:(64,'ff5531f9bb3b9c1544067cad8f464989c3a2a30ca3edb5c76cde603af70c34de'),
 0x54a834:(424,'ff7e371c0904ff272d8bc9b643ce1b17d17e3fe007097c03b2133574d44add11'),
 0x54a9dc:(232,'2f5ca294806906d944c0ae7715fb2c341febfc3614f5cdc4f7e5f7e300dbca26'),
 0x54ab9c:(204,'c25c07e30fb33a1052291c6aa6ffae115c78eccc8dee577ec74dff8e9b9feb5e'),
 0x635bb8:(56,'fc26aadf8e356b85d7b510039ffe790f20aa24b34787bc670e71ab9aa2c64f93'),
}
FIXED_STRINGS={
 0x78d53f:'SL_IID_SEEK',0x78d54b:'SL_IID_MUTESOLO',0x78d55b:'SL_IID_VOLUME',
 0x78d569:'SL_IID_PLAY',0x78d5a9:'slCreateEngine',0x78d5b8:'SL_IID_ENGINE',
 0x78d5c6:'SL_IID_ENVIRONMENTALREVERB',0x78d4ee:'AAssetManager_fromJava',
 0x78d505:'AAssetManager_open',0x78d518:'AAsset_openFileDescriptor',
 0x78d532:'AAsset_close',0x78f29d:'libc.so',0x78f2a5:'getauxval',
}
FORBIDDEN=('setScriptEngine','CCScriptEngineManager')


def cstring(e:Elf,a:int)->str:
 out=bytearray()
 for i in range(256):
  b=e.at(a+i,1)[0]
  if b==0:return out.decode('ascii')
  out.append(b)
 raise ValueError('unterminated cstring')


def relplt_symbol(e:Elf,got:int)->tuple[str,int,int]:
 rel=e.byname['.rel.plt']; dyn=e.byname['.dynsym']; st=e.byname['.dynstr']
 strings=e.data[st[4]:st[4]+st[5]]
 entries=[]
 for index,p in enumerate(range(rel[4],rel[4]+rel[5],8)):
  off,info=struct.unpack_from('<II',e.data,p)
  if off!=got:continue
  sym=info>>8; typ=info&255; sp=dyn[4]+sym*16
  no=struct.unpack_from('<I',e.data,sp)[0]; end=strings.find(b'\0',no); req(end>=0,'bad rel.plt symbol')
  entries.append((strings[no:end].decode(),typ,index))
 req(len(entries)==1,'dlsym GOT relocation not unique')
 return entries[0]


def bl_target(pc:int,a:int,b:int)->int|None:
 if (a&0xf800)!=0xf000 or (b&0xd000)!=0xd000:return None
 s=(a>>10)&1;j1=(b>>13)&1;j2=(b>>11)&1;i1=1^(j1^s);i2=1^(j2^s)
 imm=(s<<24)|(i1<<23)|(i2<<22)|((a&0x3ff)<<12)|((b&0x7ff)<<1)
 if s:imm-=1<<25
 return (pc+4+imm)&0xffffffff


def blx_target(pc:int,a:int,b:int)->int|None:
 if (a&0xf800)!=0xf000 or (b&0xd001)!=0xc000:return None
 s=(a>>10)&1;j1=(b>>13)&1;j2=(b>>11)&1;i1=1^(j1^s);i2=1^(j2^s)
 imm=(s<<24)|(i1<<23)|(i2<<22)|((a&0x3ff)<<12)|(((b>>1)&0x3ff)<<2)
 if s:imm-=1<<25
 return ((pc+4)&~3)+imm


def scan_text(e:Elf,target:int,decoder)->set[int]:
 sec=e.byname['.text']; data=e.at(sec[3],sec[5]); out=set()
 for off in range(0,len(data)-3,2):
  a,b=struct.unpack_from('<HH',data,off)
  if decoder(sec[3]+off,a,b)==target:out.add(sec[3]+off)
 return out


def progbits_refs(e:Elf,value:int)->list[dict]:
 needle=struct.pack('<I',value); out=[]
 for n,s in e.byname.items():
  if s[1]!=1:continue
  blob=e.data[s[4]:s[4]+s[5]]; p=0
  while True:
   p=blob.find(needle,p)
   if p<0:break
   out.append({'section':n,'address':f'0x{s[3]+p:08x}'});p+=1
 return out


def analyze(apk_path:Path)->dict:
 req(zipfile.is_zipfile(apk_path),'exact APK required')
 with zipfile.ZipFile(apk_path) as z:
  cocos=z.read('lib/armeabi-v7a/libcocos2dcpp.so'); ff=z.read('lib/armeabi-v7a/libffmpeg.so')
 req(hashlib.sha256(cocos).hexdigest()==SHA,'cocos ELF SHA mismatch')
 req(hashlib.sha256(ff).hexdigest()==FFMPEG_SHA,'ffmpeg ELF SHA mismatch')
 e=Elf(cocos)
 for a,(size,digest) in FUNCTION_HASHES.items():req(hashlib.sha256(e.at(a,size)).hexdigest()==digest,'function hash mismatch '+hex(a))
 for a,s in FIXED_STRINGS.items():req(cstring(e,a)==s,'fixed dynamic name changed '+hex(a))
 name,typ,index=relplt_symbol(e,DLSYM_GOT);req((name,typ)==('dlsym',22),'dlsym relocation mismatch')
 plt=e.byname['.plt'][3]+20+index*12;req(plt==0x278ee8,'dlsym PLT entry changed')
 dlsym_calls=scan_text(e,plt,blx_target);req(dlsym_calls==EXPECTED_DLSYM_CALLS,'dlsym direct call surface changed')
 req(scan_text(e,GET_INTERFACE,bl_target)==EXPECTED_INTERFACE_CALLS,'getInterfaceID caller surface changed')
 req(scan_text(e,GET_FUNC,bl_target)==EXPECTED_FUNC_CALLS,'getFuncPtr caller surface changed')
 # Neither generic helper is retained as a PROGBITS callback/function pointer.
 for value in (GET_INTERFACE,GET_INTERFACE|1,GET_FUNC,GET_FUNC|1):req(progbits_refs(e,value)==[],'OpenSL helper retained as function pointer')
 # The exact fixed caller bodies above constrain all helper arguments to these literals.
 allowed=sorted(set(FIXED_STRINGS.values())-{'libc.so'})
 req(not any(any(f in s for f in FORBIDDEN) for s in allowed),'forbidden lookup name in allowlist')
 # The second packaged native library cannot name the setter/helpers either.
 for needle in (b'setScriptEngine',b'CCScriptEngineManager',b'_Z10getFuncPtrPKc',b'_Z14getInterfaceIDPKc'):
  req(needle not in ff,'libffmpeg references Cocos dynamic lookup surface')
 return {'format':FORMAT,'cocos_sha256':SHA,'ffmpeg_sha256':FFMPEG_SHA,'dlsym_plt':f'0x{plt:08x}','direct_dlsym_calls':[f'0x{x:08x}' for x in sorted(dlsym_calls)],'direct_dlsym_call_count':len(dlsym_calls),'getInterfaceID_direct_call_count':8,'getFuncPtr_direct_call_count':1,'fixed_dynamic_names':allowed,'packaged_dlsym_lookup_of_set_script_engine_excluded':True,'packaged_cross_library_lookup_of_set_script_engine_excluded':True,'external_injected_code_excluded':False,'whole_program_alias_writes_excluded':False,'receiver_class_proven':False,'affects_routing':False}


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('original_apk',type=Path);p.add_argument('--details-output',type=Path,required=True);a=p.parse_args();r=analyze(a.original_apk);a.details_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.write_text(json.dumps(r,sort_keys=True)+'\n');print('format:',FORMAT);print('dlsym calls:',r['direct_dlsym_call_count']);print('dynamic names:',r['fixed_dynamic_names']);print('packaged dlsym setter lookup excluded: yes');print('injected/external runtime code excluded: no');print('affects routing: no');print('details:',a.details_output)
if __name__=='__main__':main()
