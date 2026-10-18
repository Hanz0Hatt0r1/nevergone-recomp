# ManagementLayer announcement callback

This note records the original announcement callback contract and its current
clean-room UI handoff. The target is Never Gone 1.0.9 ARMv7
`libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Recovered input and native route

The original `UILogin.lua` calls:

```text
cpp_OnGameAnnoucement(0, 0, AnnouncementData.Title, AnnouncementData.Content)
```

The ARMv7 `LUA_LOGIN::cpp_OnGameAnnoucement(lua_State*)` at ELF address
`0x002e8e7c` reads Lua arguments 1 and 2 as numbers, arguments 3 and 4 as
strings, and forwards them to `LUA_LOGIN::GameAnnoucement(int, int, CCString,
CCString)` at `0x002e8dcc`.

`GameAnnoucement` stores the two integers at `ManagementLayer +0x798` and
`+0x764`, and copies the title and content into strings at `+0x768` and
`+0x780`. It then updates the two-stage announcement frame. The login Lua
callback sends zero for both integer fields.

`ManagementLayer::ShowAnnouncement()` at `0x002f7674` removes its prior
announcement layer from `+0x79c`, creates an `Announcement`, supplies the
stored title and content, and adds the layer at z-order 10. `Announcement::init`
at `0x0035f948` initializes a black layer with alpha 229 and registers touch
handling. Its frame builder takes both strings and installs the recovered
`ManagementLayer::ShowAnnouncement` callback on the continue control.

## Project UI handoff

The callback bridge now keeps the raw arguments plus separate title/content
fields. `MainActivity` presents those strings in an Android `AlertDialog`; OK
or Back dismisses it. The reconstructed Management route remembers the route
that was active before the announcement and restores it when the dialog closes.
This allows a server-list callback already received before the announcement to
become visible again.

The Android dialog is a functional clean-room presentation. It does not claim
pixel parity with `Announcement::MiniFrame`; the original sprite paths, exact
frame geometry, localization, and queued-announcement sequencing remain open.

Function labels for the recovered callback/route are stored in
`docs/evidence/manual-nevergone-recoveries/management-announcement-labels.jsonl`.
