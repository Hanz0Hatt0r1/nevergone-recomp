#!/usr/bin/env python3
from __future__ import annotations
import audit_nevergone_controls_full_this_static_closure as m

def main():
    assert len(m.DIRECT)==11 and len(m.INDIRECT)==3
    assert set(m.DIRECT)&set(m.INDIRECT)==set()
    assert {x[2] for x in m.DIRECT.values()}=={
        'lifetime_scheduler_cleanup','base_destructor','terminal_delete','base_constructor',
        'scheduler_retained_callback','action_retained_callback','button_retained_callback'}
    assert {x[3] for x in m.INDIRECT.values()}=={'parent_backref','touch_delegate_backref'}
    assert m.bl_target(0x296728,0xf280,0xfc4c)==0x516fc4
    assert m.bl_target(0x2979c2,0xf277,0xfd34)==0x50f42e
    assert m.bl_target(0x298118,0xf000,0xfa70)==0x2985fc
    assert m.bl_target(0x2982b2,0xf27e,0xfe47)==0x516f44
    assert not ({0xe0,0xe4,0xe8,0xec,0xf0,0xf4,0xf8,0xfc,0x100,0xd1}&m.DANGEROUS)
    print('Controls full-this static closure smoke: ok')

if __name__=='__main__':main()
