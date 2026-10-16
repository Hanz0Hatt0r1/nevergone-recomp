#!/usr/bin/env python3
"""Check exact frame membership in the shipped legacy plist shape."""

from diagnose_player_atlas_family import LEGACY_PLIST_DTD, plist_membership


payload = (
    b'<?xml version="1.0"?>' + LEGACY_PLIST_DTD +
    b'<plist version="1.0"><dict><key>frames</key><dict>'
    b'<key>02P01_I01_001.png</key><dict><key>aliases</key><array/></dict>'
    b'</dict><key>metadata</key><dict><key>format</key><integer>3</integer>'
    b'</dict></dict></plist>'
)
exact, casefold_matches, aliases, count, fmt = plist_membership(
    payload, "P01_I01_001.png")
assert not exact and not casefold_matches and not aliases
assert count == 1 and fmt == 3
