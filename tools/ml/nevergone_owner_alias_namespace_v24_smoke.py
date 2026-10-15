#!/usr/bin/env python3
"""Source-level regression smoke for namespaced owner matching in the Ghidra exporter."""
from pathlib import Path
import sys

path=Path(sys.argv[1]) if len(sys.argv)>1 else Path('tools/ghidra/ExportClassOwnerAliasFlow.java')
s=path.read_text(encoding='utf-8')
need=[
    'int split=cls.lastIndexOf("::")',
    'String shortCls=split<0?cls:cls.substring(split+2)',
    'String shortWant=(shortCls+"*").replace(" ","")',
    'if (!("this".equals(name) || "__this".equals(name))) return false',
    'type.equals(shortWant)',
]
for x in need:
    assert x in s,x
pos_this=s.index('if (!("this".equals(name) || "__this".equals(name))) return false')
pos_short=s.index('type.equals(shortWant)')
assert pos_this < pos_short
assert 'if (type.equals(want) || type.endsWith("::"+want)) return true;' in s
print('owner alias namespace v24 smoke: ok')
