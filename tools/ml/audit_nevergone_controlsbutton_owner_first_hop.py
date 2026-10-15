#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,hashlib,json,re,struct,tarfile,zipfile,io
from collections import Counter
from pathlib import Path

SHA='94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e'
FORMAT='NEVERGONE.ControlsButtonOwnerFieldFirstHop/1'
OWNER_AUDIT='NEVERGONE.ControlsButtonOwnerFieldAliasAudit/1'
INV='NEVERGONE.GhidraControlsButtonOwnerFieldAliasInventory/1'
TARGET_FIELDS={0x104,0x108,0x10c}
EXPECTED_UNKNOWN=('002a865a','002a865e')
EXPECTED_DIRECT={
 '005353f8':'_ZN7cocos2d7CCLayerD2Ev',
 '0070ea70':'_ZdlPv',
 '00535b2a':'_ZN7cocos2d7CCLayer7onEnterEv',
 '00535b7c':'_ZN7cocos2d7CCLayer6onExitEv',
 '00535204':'_ZN7cocos2d7CCLayerC2Ev',
}
EXPECTED_INDIRECT={
 '002a84d8':0xb8,'002a84f4':0xb8,'002a851a':0xb8,'002a85d0':0xb8,'002a85e8':0xb8,
 '002a8712':0x84,'002a8814':0x84,'002a8848':0x100,'002a8852':0x100,'002a893c':0x84,
}
SLOT_TARGETS={
 0x84:'_ZN7cocos2d6CCNode9isVisibleEv',
 0xb8:'_ZN7cocos2d6CCNode8addChildEPS0_',
 0x100:'_ZN7cocos2d6CCNode6getTagEv',
}


def req(c,m):
    if not c: raise ValueError(m)

class Elf:
    def __init__(self,data:bytes):
        req(hashlib.sha256(data).hexdigest()==SHA,'ELF SHA mismatch')
        req(data[:6]==b'\x7fELF\x01\x01','not ELF32 LE')
        self.data=data
        shoff=struct.unpack_from('<I',data,32)[0]
        ents,n,si=struct.unpack_from('<HHH',data,46)
        req(ents==40 and 0<n<256,'bad section table')
        self.sections=[struct.unpack_from('<IIIIIIIIII',data,shoff+i*ents) for i in range(n)]
        shstr=self.sections[si]; names=data[shstr[4]:shstr[4]+shstr[5]]
        self.byname={}
        for s in self.sections:
            e=names.find(b'\0',s[0]); req(e>=0,'bad section name')
            self.byname[names[s[0]:e].decode()]=s
        ds=self.byname['.dynsym']; st=self.byname['.dynstr']; strings=data[st[4]:st[4]+st[5]]
        self.symbols={}; self.by_addr={}
        for p in range(ds[4],ds[4]+ds[5],16):
            no,v,z,info,_,sh=struct.unpack_from('<IIIBBH',data,p)
            e=strings.find(b'\0',no); req(e>=0,'bad symbol name')
            name=strings[no:e].decode('utf-8','replace'); typ=info&15
            if name:
                self.symbols[name]=(v,z,typ,sh)
                if typ==2 and v:
                    self.by_addr.setdefault(v&~1,[]).append(name)
        self.rels={}
        for s in self.sections:
            if s[1]!=9: continue
            req(s[9]==8 and s[5]%8==0,'bad REL section')
            for p in range(s[4],s[4]+s[5],8):
                a,i=struct.unpack_from('<II',data,p); self.rels[a]=i&255
    def at(self,a,n):
        m=[s for s in self.sections if s[1]==1 and s[3]<=a and a+n<=s[3]+s[5]]
        req(len(m)==1,f'unmapped ELF VA {a:08x}')
        s=m[0]; o=s[4]+a-s[3]; return self.data[o:o+n]
    def u32(self,a): return struct.unpack('<I',self.at(a,4))[0]
    def sym(self,n):
        req(n in self.symbols,'missing symbol '+n)
        return self.symbols[n]


def read_member(tf,name):
    try: f=tf.extractfile(name)
    except KeyError: f=None
    req(f is not None,'archive missing '+name)
    return f.read()

def tsv_bytes(b):
    return list(csv.DictReader(io.StringIO(b.decode()),delimiter='\t'))
def parse_offsets(s):
    if not s or s=='-': return []
    return [x.strip() for x in s.split(',')]
def parse_direct(tok):
    m=re.fullmatch(r'address:([0-9a-f]{8}):(?:4|8)',tok or '')
    return m.group(1) if m else None
def ghidra_to_elf(a):
    return int(a,16)-0x10000


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('apk',type=Path); ap.add_argument('archive',type=Path)
    ap.add_argument('--details-output',type=Path,required=True)
    a=ap.parse_args()
    with zipfile.ZipFile(a.apk) as z: elf=Elf(z.read('lib/armeabi-v7a/libcocos2dcpp.so'))
    with tarfile.open(a.archive,'r:gz') as tf:
        audit=json.loads(read_member(tf,'controlsbutton-owner-field-alias-audit.jsonl').decode().strip())
        methods=tsv_bytes(read_member(tf,'controlsbutton-owner-methods.tsv'))
        rows=tsv_bytes(read_member(tf,'evidence/controlsbutton-owner-field-aliases.tsv'))
    req(audit['format']==OWNER_AUDIT and audit['status']=='bounded_controlsbutton_owner_probe_complete','owner audit mismatch')
    req(audit['methods_expected']==19 and audit['methods_decompiled']==19,'method count mismatch')
    req(audit['all_stores']==52 and audit['all_calls']==107,'operation count mismatch')
    req(audit['unknown_store_count']==1,'unknown store count changed')
    req(audit['parameter_zero_call_count']==32,'param0 call count changed')
    req(audit['parameter_zero_argument_offsets']=={'0x0':23,'0x120':7,'0xe0':2},'param0 offset split changed')
    method_entries={r['function_entry'] for r in methods}
    req(len(method_entries)==19,'method manifest mismatch')

    unk=[r for r in rows if r['row_type']=='store' and r['address_kind'].startswith('opaque_')]
    req(len(unk)==1 and (unk[0]['function_entry'],unk[0]['op_address'])==EXPECTED_UNKNOWN,'unknown store identity changed')
    raw=elf.at(0x29865e,4)
    h1,h2=struct.unpack('<HH',raw)
    req(h1==0xf884 and (h2&0xfff)==0x11c and (h2>>12)==1,'setEnabled exact store changed')

    full=[]
    for r in rows:
        if r.get('row_type')!='call': continue
        idx=parse_offsets(r.get('param0_arg_indices')); off=parse_offsets(r.get('param0_arg_offsets'))
        req(len(idx)==len(off),'param0 index/offset arity mismatch')
        if '0x0' in off:
            full.append((r,idx[off.index('0x0')]))
    req(len(full)==23,'full-this edge count changed')
    internal=[]; direct=[]; indirect=[]
    for r,arg in full:
        tgt=parse_direct(r['call_target_token']) if r['call_kind']=='CALL' else None
        if tgt in method_entries:
            internal.append((r,arg)); continue
        if r['call_kind']=='CALL' and tgt:
            direct.append((r,arg,tgt)); continue
        if r['call_kind']=='CALLIND':
            indirect.append((r,arg)); continue
        raise ValueError('unexpected external full-this edge '+r['op_address'])
    req(len(internal)==8 and len(direct)==5 and len(indirect)==10,'full-this split changed')

    direct_detail=[]
    req({t for _,_,t in direct}==set(EXPECTED_DIRECT),'direct external targets changed')
    for r,arg,t in direct:
        elf_addr=ghidra_to_elf(t); expected=EXPECTED_DIRECT[t]
        req(expected in elf.by_addr.get(elf_addr,[]),'direct target symbol mismatch '+t)
        direct_detail.append({'source':r['function_entry']+':'+r['op_address'],'target':t,'elf_target':f'{elf_addr:08x}','symbol':expected,'argument_index':int(arg)})

    indirect_detail=[]; counts=Counter()
    req({r['op_address'] for r,_ in indirect}==set(EXPECTED_INDIRECT),'indirect site set changed')
    vraw,vsz,typ,sh=elf.sym('_ZTV14ControlsButton'); req((vraw&~1)==0x8e46f0 and vsz==560,'ControlsButton vtable changed')
    vptr=0x8e46f8
    for r,arg in indirect:
        site=r['op_address']; slot=EXPECTED_INDIRECT[site]; pc=ghidra_to_elf(site)
        matched=False
        for back in (4,6,8):
            first,second=struct.unpack('<HH',elf.at(pc-back,4))
            if first==0xf8d3 and (second&0xfff)==slot:
                matched=True; break
        req(matched,'indirect slot load changed '+site)
        req(elf.at(pc,2)==b'\x98\x47','indirect BLX changed '+site)
        sym=SLOT_TARGETS[slot]; rawval,_,_,_=elf.sym(sym); entry=vptr+slot
        req(elf.rels.get(entry)==23,'vtable relocation changed '+hex(slot))
        req(elf.u32(entry)==rawval,'vtable target changed '+hex(slot))
        counts[hex(slot)]+=1
        indirect_detail.append({'source':r['function_entry']+':'+site,'slot':hex(slot),'symbol':sym,'argument_index':int(arg)})
    req(counts==Counter({'0xb8':5,'0x84':3,'0x100':2}),'indirect slot counts changed')

    req(elf.at(0x515d28,6).hex()=='90f8d0007047','isVisible body changed')
    req(elf.at(0x515d70,6).hex()=='d0f8ac007047','getTag body changed')
    req(elf.at(0x516b5c,22).hex()=='10b50446042003f027fbc4f8a400bde8104002f0b0be','childrenAlloc body changed')
    req(elf.at(0x516c42,34).hex()=='38b5012380f8d2300c46d0f8a4301546586926f0effd236820462946db6a984738bd','insertChild body changed')
    add_raw,add_sz,_,_=elf.sym('_ZN7cocos2d6CCNode8addChildEPS0_ii')
    req((add_raw&~1)==0x516c64 and add_sz==100,'addChild core symbol changed')
    owner_write_offsets=['0xa4','0xd2']
    req(not (TARGET_FIELDS & {int(x,16) for x in owner_write_offsets}),'addChild writes target fields')

    detail={
      'format':FORMAT,'status':'controlsbutton_owner_first_hop_classified','elf_sha256':SHA,'methods_complete':19,
      'local_unknown_store':{'function':'ControlsButton::setEnabled','op_address':'002a865e','field_offset':'0x11c','target_field':False},
      'full_this_edge_count':23,'internal_controlsbutton_edge_count':8,
      'external_full_this_edge_count':15,'external_direct_edge_count':5,'external_indirect_edge_count':10,
      'direct_external':direct_detail,'indirect_external':indirect_detail,'indirect_slot_counts':dict(sorted(counts.items())),
      'simple_read_only_virtuals':['CCNode::isVisible','CCNode::getTag'],
      'addchild_owner_direct_write_offsets':owner_write_offsets,
      'target_fields_written_by_first_hop_external_targets':False,
      'remaining_alias_frontier':['ControlsButton+0xe0 targeted-touch delegate retention','CCNode child+0xa8 parent back-references from addChild','CCLayer lifecycle nested/retained base paths'],
      'owner_fields_stable':False,'receiver_class_proven':False,'affects_routing':False,
    }
    a.details_output.parent.mkdir(parents=True,exist_ok=True)
    a.details_output.write_text(json.dumps(detail,sort_keys=True)+'\n')
    print('format:',FORMAT)
    print('status:',detail['status'])
    print('local unknown STORE: 002a865e -> ControlsButton+0x11c (non-target)')
    print('full ControlsButton this:',23,'internal=',8,'external=',15)
    print('external split: direct=',5,'indirect=',10)
    print('indirect slots:',dict(sorted(counts.items())))
    print('target-field first-hop writers: 0')
    print('remaining alias frontier:',len(detail['remaining_alias_frontier']))
    for x in detail['remaining_alias_frontier']: print(' -',x)
    print('owner field stability: not yet proven; affects routing: no')
    print('details:',a.details_output)
if __name__=='__main__': main()
