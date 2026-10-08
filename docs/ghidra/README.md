# Nevergone Ghidra source index

Source inspected: the user-provided local Ghidra folder on 2026-10-08. It contains a Ghidra project (`nevergone.gpr` and `nevergone.rep`), a Ghidra program archive (`nev.gzf`), the original APK, and an XAPK with OBB. The Ghidra project has an analyzed ARMv7 `libcocos2dcpp.so` and `libffmpeg.so`. It does not contain a prebuilt JSON or CSV export.

[ghidra-login-index.tsv](ghidra-login-index.tsv) records matching Ghidra function names and defined strings with their addresses and reference addresses. It was generated with [NevergoneIndex.java](../../tools/ghidra/NevergoneIndex.java) from a temporary copy of the Ghidra project opened read-only. No original image, audio, binary, or decompiled code is stored here. Ghidra's loaded program addresses are 0x10000 above the ELF virtual addresses in the APK.

## SingleLogin evidence

| Item | Ghidra evidence | Atlas metadata |
| --- | --- | --- |
| `zjmyun01.png` | string `0x0078c5d5`, referenced at `0x00435b00` in `SingleLoginLayer::InitUI` (`0x00435a40`) | `SingleLogin_default.plist`, 721 × 278, rect `{{1138, 0}, {721, 278}}` |
| `zjmyun02.png` | string `0x0078c5e2`, referenced at `0x00435b0a` in `InitUI` | same atlas, 720 × 277, rect `{{1138, 280}, {720, 277}}` |
| `zjmdengguang01…04.png` | strings `0x0078c5fe`–`0x0078c637`, referenced at `0x00436384`–`0x004363a2` in `InitUI` | same atlas, four trimmed frames |

`InitUI` creates three instances of each cloud sprite at layer depths 3, 2, and 1. The code applies `CCMoveTo` sequences with `CCRepeatForever` to them. It creates the light sprites afterward with delay and fade actions. These cloud and light actions are already represented in the current repository timeline code.

The Ghidra `InitUI` decompilation also confirms a later lightning block: seven `zjmshandian01…07.png` nodes and four `zjmjianzhuzhaoliang01…04.png` illumination nodes. The first six lightning nodes have z=3, the seventh has z=0, and all four illumination nodes have z=4. The z values follow from the initialized `InitUI` arrays; the illumination z array is `{4,4,4,4}` at Ghidra address `0x007a33e0`. Each lightning node is associated with four randomly selected illumination nodes and a delay/fade action chain. The seven lightning frames and four illumination frames all use a 1136 × 640 TexturePacker source size in `SingleLogin_default.plist`.

The source APK contains the `SingleLogin_default.plist/png` pair and the referenced frames. Only the user-imported APK should supply these assets at runtime.

## Repository comparison

The private `Hanz0Hatt0r1/nevergone-recomp` repository was fetched on 2026-10-08. At indexing time, `main` was `38a9ef5` (PR #68). It already contained the nine cloud motion timelines and atlas rendering (`#64–65`), animated light effects (`#56`), buildings (`#57`), foreground/tree sway (`#66–67`), and a host-tested lightning event timeline (`#68`). The seven lightning frames and four illumination frames were not yet loaded or drawn by `SingleLoginAtlasComposer`/`single_login_compositor.cpp`. That renderer connection is the next small implementation step.
