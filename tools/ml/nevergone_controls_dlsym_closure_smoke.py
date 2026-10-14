#!/usr/bin/env python3
from __future__ import annotations
import audit_nevergone_controls_dlsym_closure as m

def main():
    assert m.bl_target(0x54aa04,0xf7ff,0xfed6)==m.GET_INTERFACE
    assert m.bl_target(0x54abe0,0xf7ff,0xfe08)==m.GET_FUNC
    assert m.blx_target(0x54a806,0xf52e,0xeb70)==0x278ee8
    assert m.blx_target(0x635bd2,0xf443,0xe98a)==0x278ee8
    assert len(m.EXPECTED_DLSYM_CALLS)==7
    assert len(m.EXPECTED_INTERFACE_CALLS)==8
    assert m.EXPECTED_FUNC_CALLS=={0x54abe0}
    assert 'slCreateEngine' in m.FIXED_STRINGS.values()
    assert 'getauxval' in m.FIXED_STRINGS.values()
    assert not any('setScriptEngine' in s for s in m.FIXED_STRINGS.values())
    print('Controls dlsym closure smoke: ok')

if __name__=='__main__': main()
