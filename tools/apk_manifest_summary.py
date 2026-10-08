#!/usr/bin/env python3
"""Decode an APK's binary AndroidManifest.xml into metadata-only JSON/Markdown."""
from __future__ import annotations

import argparse, hashlib, json, struct, zipfile
from pathlib import Path
import xml.etree.ElementTree as ET
from typing import Any

ANDROID_NS = "http://schemas.android.com/apk/res/android"
A = f"{{{ANDROID_NS}}}"
NO = 0xFFFFFFFF
XML, STRINGS, START_NS, END_NS, START, END = 0x0003, 0x0001, 0x0100, 0x0101, 0x0102, 0x0103
UTF8 = 0x100

class AxmlError(ValueError): pass

def u16(b: bytes, o: int) -> int:
    if o < 0 or o + 2 > len(b): raise AxmlError("truncated uint16")
    return struct.unpack_from("<H", b, o)[0]

def u32(b: bytes, o: int) -> int:
    if o < 0 or o + 4 > len(b): raise AxmlError("truncated uint32")
    return struct.unpack_from("<I", b, o)[0]

def chunk(b: bytes, o: int) -> tuple[int,int,int]:
    if o < 0 or o + 8 > len(b): raise AxmlError("truncated chunk header")
    t,h,n = u16(b,o),u16(b,o+2),u32(b,o+4)
    if h < 8 or n < h or o+n > len(b): raise AxmlError(f"invalid chunk at 0x{o:x}")
    return t,h,n

def len8(b: bytes, o: int) -> tuple[int,int]:
    if o >= len(b): raise AxmlError("truncated UTF-8 length")
    x=b[o]; o+=1
    if x&0x80:
        if o>=len(b): raise AxmlError("truncated UTF-8 length")
        x=((x&0x7f)<<8)|b[o]; o+=1
    return x,o

def len16(b: bytes, o: int) -> tuple[int,int]:
    x=u16(b,o); o+=2
    if x&0x8000: x=((x&0x7fff)<<16)|u16(b,o); o+=2
    return x,o

class Pool:
    def __init__(self,b: bytes,o: int):
        t,h,n=chunk(b,o)
        if t!=STRINGS or h<28: raise AxmlError("bad string pool")
        count,styles,flags,start,styles_start = (u32(b,o+x) for x in (8,12,16,20,24))
        table=o+h
        if table+(count+styles)*4>o+n: raise AxmlError("bad string offset table")
        base=o+start; limit=o+(styles_start or n)
        if not o<=base<=limit<=o+n: raise AxmlError("bad string payload bounds")
        self.s=[]; utf8=bool(flags&UTF8)
        for i in range(count):
            p=base+u32(b,table+i*4)
            if not base<=p<limit: raise AxmlError("string offset out of bounds")
            if utf8:
                _,p=len8(b,p); size,p=len8(b,p); end=p+size
                if end>=limit: raise AxmlError("truncated UTF-8 string")
                s=b[p:end].decode("utf-8","replace")
            else:
                size,p=len16(b,p); end=p+size*2
                if end+2>limit: raise AxmlError("truncated UTF-16 string")
                s=b[p:end].decode("utf-16le","replace")
            self.s.append(s)
    def get(self,i: int) -> str|None:
        if i==NO: return None
        if i>=len(self.s): raise AxmlError("string index out of bounds")
        return self.s[i]

def typed(p: Pool, raw: int, typ: int, data: int) -> str:
    s=p.get(raw)
    if s is not None: return s
    if typ==0x03: return p.get(data) or ""
    if typ==0x12: return "true" if data else "false"
    if typ==0x10: return str(struct.unpack("<i",struct.pack("<I",data))[0])
    if typ==0x11: return f"0x{data:08x}"
    if typ==0x01: return f"@0x{data:08x}"
    if typ==0x02: return f"?0x{data:08x}"
    if typ==0x04: return repr(struct.unpack("<f",struct.pack("<I",data))[0])
    if 0x1c<=typ<=0x1f: return f"#{data:08x}"
    return f"0x{data:08x}"

def parse_manifest(data: bytes) -> ET.Element:
    if data.lstrip().startswith(b"<"):
        try: return ET.fromstring(data)
        except ET.ParseError as e: raise AxmlError(str(e)) from e
    t,h,total=chunk(data,0)
    if t!=XML: raise AxmlError("not Android binary XML")
    pool=None; stack=[]; root=None; o=h
    while o<total:
        t,h,n=chunk(data,o)
        if t==STRINGS: pool=Pool(data,o)
        elif t in (START_NS,END_NS):
            if n<24: raise AxmlError("truncated namespace")
        elif t==START:
            if pool is None or n<36: raise AxmlError("invalid start element")
            ns,name=pool.get(u32(data,o+16)),pool.get(u32(data,o+20))
            if name is None: raise AxmlError("unnamed element")
            ast,asz,ac=u16(data,o+24),u16(data,o+26),u16(data,o+28)
            apos=o+16+ast
            if asz<20 or apos+ac*asz>o+n: raise AxmlError("bad attributes")
            e=ET.Element(f"{{{ns}}}{name}" if ns else name)
            for i in range(ac):
                p=apos+i*asz; ans=pool.get(u32(data,p)); an=pool.get(u32(data,p+4))
                if an is None: raise AxmlError("unnamed attribute")
                key=f"{{{ans}}}{an}" if ans else an
                e.set(key,typed(pool,u32(data,p+8),data[p+15],u32(data,p+16)))
            if stack: stack[-1].append(e)
            elif root is None: root=e
            else: raise AxmlError("multiple roots")
            stack.append(e)
        elif t==END:
            if not stack: raise AxmlError("unmatched end element")
            stack.pop()
        o+=n
    if root is None or stack: raise AxmlError("incomplete manifest")
    return root

def av(e: ET.Element,name: str) -> str|None: return e.attrib.get(A+name)
def cname(pkg: str,name: str|None) -> str|None:
    if not name:return name
    if name.startswith("."):return pkg+name
    if "." not in name:return pkg+"."+name
    return name

def launcher(e: ET.Element) -> bool:
    for f in e.findall("intent-filter"):
        acts={av(x,"name") for x in f.findall("action")}; cats={av(x,"name") for x in f.findall("category")}
        if "android.intent.action.MAIN" in acts and "android.intent.category.LAUNCHER" in cats:return True
    return False

def summarize(root: ET.Element) -> dict[str,Any]:
    if root.tag!="manifest": raise AxmlError("unexpected root")
    pkg=root.attrib.get("package",""); sdk=root.find("uses-sdk"); app=root.find("application")
    features=[]
    for x in root.findall("uses-feature"):
        d={k:v for k in ("name","required","glEsVersion") if (v:=av(x,k)) is not None}
        if (g:=d.get("glEsVersion")) and g.startswith("0x"):
            try:q=int(g,16); d["glEsVersionDecoded"]=f"{q>>16}.{q&0xffff}"
            except ValueError:pass
        features.append(d)
    comps={"activities":[],"services":[],"receivers":[],"providers":[]}; launches=[]; metadata=[]; appinfo={}
    if app is not None:
        for k in ("name","label","icon","theme","debuggable"):
            if (v:=av(app,k)) is not None: appinfo[k]=v
        for x in app.findall("meta-data"):
            d={"name":av(x,"name") or ""}
            for k in ("value","resource"):
                if (v:=av(x,k)) is not None:d[k]=v
            metadata.append(d)
        for tag,key in (("activity","activities"),("activity-alias","activities"),("service","services"),("receiver","receivers"),("provider","providers")):
            for x in app.findall(tag):
                name=cname(pkg,av(x,"name")); d={"name":name or ""}
                for k in ("exported","screenOrientation","configChanges","permission"):
                    if (v:=av(x,k)) is not None:d[k]=v
                if tag.startswith("activity") and launcher(x): d["launcher"]=True; launches.append(name or "")
                comps[key].append(d)
    screens={}
    if (x:=root.find("supports-screens")) is not None:
        for k in ("smallScreens","normalScreens","largeScreens","xlargeScreens","anyDensity","resizeable"):
            if (v:=av(x,k)) is not None:screens[k]=v
    sd={}
    if sdk is not None:
        for k in ("minSdkVersion","targetSdkVersion","maxSdkVersion"):
            if (v:=av(sdk,k)) is not None:sd[k]=v
    return {"package":pkg,"versionCode":av(root,"versionCode"),"versionName":av(root,"versionName"),
            "platformBuildVersionCode":root.attrib.get("platformBuildVersionCode"),"platformBuildVersionName":root.attrib.get("platformBuildVersionName"),
            "sdk":sd,"features":features,"permissions":[v for x in root.findall("uses-permission") if (v:=av(x,"name"))],
            "supportsScreens":screens,"application":appinfo,"applicationMetadata":metadata,"launcherActivities":launches,"components":comps}

def sha256(path: Path) -> str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda:f.read(1<<20),b""):h.update(b)
    return h.hexdigest()

def summarize_apk(path: Path) -> dict[str,Any]:
    with zipfile.ZipFile(path) as z:
        try:data=z.read("AndroidManifest.xml")
        except KeyError as e:raise AxmlError("APK lacks AndroidManifest.xml") from e
    s=summarize(parse_manifest(data)); s["apk"]={"path":str(path),"size":path.stat().st_size,"sha256":sha256(path),"manifestBinarySize":len(data)}; return s

def markdown(s: dict[str,Any]) -> str:
    sdk=s["sdk"]; a=s.get("apk",{}); out=["# Android manifest summary","",f"- APK SHA-256: `{a.get('sha256','unknown')}`",f"- Package: `{s['package']}`",f"- Version: `{s.get('versionName') or 'unknown'}` (`versionCode {s.get('versionCode') or 'unknown'}`)",f"- minSdkVersion: `{sdk.get('minSdkVersion','unspecified')}`",f"- targetSdkVersion: `{sdk.get('targetSdkVersion','unspecified')}`"]
    if s.get("platformBuildVersionCode"):out.append(f"- Platform build SDK: `{s['platformBuildVersionCode']}`")
    if s.get("platformBuildVersionName"):out.append(f"- Platform build version: `{s['platformBuildVersionName']}`")
    out += ["","## Launchers",""] + ([f"- `{x}`" for x in s["launcherActivities"]] or ["- none"])
    out += ["","## Features",""]
    for f in s["features"]: out.append(f"- OpenGL ES `{f['glEsVersionDecoded']}` (encoded `{f['glEsVersion']}`)" if "glEsVersionDecoded" in f else f"- `{f.get('name',json.dumps(f,sort_keys=True))}`")
    if not s["features"]:out.append("- none")
    out += ["","## Permissions",""] + ([f"- `{x}`" for x in s["permissions"]] or ["- none"])
    out += ["","## Screen support",""] + ([f"- {k}: `{v}`" for k,v in s["supportsScreens"].items()] or ["- unspecified"])
    out += ["","## Application components",""]
    for key,title in (("activities","Activities"),("services","Services"),("receivers","Receivers"),("providers","Providers")):
        out += [f"### {title}",""]
        items=s["components"][key]
        out += [f"- `{x['name']}`"+(" (launcher)" if x.get("launcher") else "") for x in items] if items else ["- none"]
        out.append("")
    if s["applicationMetadata"]:
        out += ["## Application metadata",""]
        for x in s["applicationMetadata"]:
            v=x.get("value") or x.get("resource"); out.append(f"- `{x['name']}`"+(f": `{v}`" if v else ""))
    return "\n".join(out).rstrip()+"\n"

def main() -> int:
    p=argparse.ArgumentParser(description=__doc__); p.add_argument("apk",type=Path); p.add_argument("--json",dest="jp",type=Path); p.add_argument("--markdown",dest="mp",type=Path); a=p.parse_args(); s=summarize_apk(a.apk)
    if a.jp:a.jp.parent.mkdir(parents=True,exist_ok=True); a.jp.write_text(json.dumps(s,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    if a.mp:a.mp.parent.mkdir(parents=True,exist_ok=True); a.mp.write_text(markdown(s),encoding="utf-8")
    if not a.jp and not a.mp:print(json.dumps(s,indent=2,ensure_ascii=False))
    return 0
if __name__=="__main__":raise SystemExit(main())
