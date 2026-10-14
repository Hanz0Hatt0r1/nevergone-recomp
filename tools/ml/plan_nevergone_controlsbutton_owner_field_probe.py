#!/usr/bin/env python3
"""Emit exact ControlsButton method manifest and native target-field STORE inventory."""
from __future__ import annotations
import argparse,csv,hashlib,json,struct,zipfile
from pathlib import Path
from audit_nevergone_controlsbutton_field_receiver_witness import OriginalElf,require

FORMAT='NEVERGONE.ControlsButtonOwnerFieldProbePlan/1'
SHA='94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e'
FIELDS={0x104,0x108,0x10c}
EXPECTED_SYMBOLS=21;EXPECTED_ENTRIES=19
EXPECTED={
 0x298494:(0,5,0x104),0x29849a:(0,5,0x108),0x2984a8:(0,5,0x10c),
 0x2984cc:(4,0,0x104),0x2984ea:(4,0,0x108),0x298510:(4,0,0x10c),
 0x298596:(0,5,0x104),0x29859e:(0,5,0x108),0x2985a4:(0,5,0x10c),
 0x2985c4:(4,0,0x104),0x2985de:(4,0,0x108),
}

def symbols(elf:OriginalElf):
 data=elf.section_bytes('.dynsym');strings=elf.section_bytes('.dynstr');out=[]
 for pos in range(0,len(data),16):
  no,val,size,info,_other,_idx=struct.unpack_from('<IIIBBH',data,pos)
  if (info&15)!=2 or not val or not size:continue
  require(no<len(strings),'bad dynsym name offset');end=strings.find(b'\0',no);require(end>=0,'unterminated dynsym')
  name=strings[no:end].decode('utf-8','replace')
  if name.startswith('_ZN14ControlsButton') or name.startswith('_ZNK14ControlsButton'):
   out.append((val&~1,size,name))
 return out

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('original_apk_or_elf',type=Path);p.add_argument('--methods-output',required=True,type=Path);p.add_argument('--details-output',required=True,type=Path);a=p.parse_args()
 if zipfile.is_zipfile(a.original_apk_or_elf):
  with zipfile.ZipFile(a.original_apk_or_elf) as z:data=z.read('lib/armeabi-v7a/libcocos2dcpp.so')
 else:data=a.original_apk_or_elf.read_bytes()
 require(hashlib.sha256(data).hexdigest()==SHA,'original ELF SHA mismatch');elf=OriginalElf(data);syms=symbols(elf);entries=sorted({x[0] for x in syms})
 require(len(syms)==EXPECTED_SYMBOLS and len(entries)==EXPECTED_ENTRIES,'ControlsButton symbol/method count changed')
 observed={}
 for start,size,name in syms:
  for pc in range(start,start+size-3,2):
   h1,h2=struct.unpack('<HH',elf.at(pc,4));off=h2&0xfff
   if (h1&0xfff0)==0xf8c0 and off in FIELDS:observed.setdefault(pc,(h1&15,h2>>12,off))
 require(observed==EXPECTED,'ControlsButton direct target-field STORE inventory changed')
 manifest=[]
 for entry in entries:
  names=sorted(name for start,_size,name in syms if start==entry)
  manifest.append({'function_entry':f'{entry+0x10000:08x}','elf_entry':f'{entry:08x}','symbol_names':';'.join(names)})
 summary={'format':FORMAT,'record_type':'summary','elf_sha256':SHA,'controlsbutton_symbol_count':len(syms),'controlsbutton_unique_method_count':len(entries),'controlsbutton_method_entries':[r['function_entry'] for r in manifest],'target_fields':['0x104','0x108','0x10c'],'direct_target_field_store_count':len(observed),'direct_target_field_stores':[{'ghidra_address':f'{pc+0x10000:08x}','elf_address':f'{pc:08x}','base_register':f'r{v[0]}','value_register':f'r{v[1]}','field_offset':hex(v[2])} for pc,v in sorted(observed.items())],'affects_routing':False}
 a.methods_output.parent.mkdir(parents=True,exist_ok=True);a.details_output.parent.mkdir(parents=True,exist_ok=True)
 with a.methods_output.open('w',encoding='utf-8',newline='') as h:
  w=csv.DictWriter(h,fieldnames=['function_entry','elf_entry','symbol_names'],delimiter='\t');w.writeheader();w.writerows(manifest)
 a.details_output.write_text(json.dumps(summary,sort_keys=True)+'\n',encoding='utf-8')
 print('format:',FORMAT);print('ControlsButton symbols / unique methods:',len(syms),'/',len(entries));print('direct target-field STOREs:',len(observed));print('methods:',a.methods_output);print('details:',a.details_output);print('affects routing: no')
if __name__=='__main__':main()
