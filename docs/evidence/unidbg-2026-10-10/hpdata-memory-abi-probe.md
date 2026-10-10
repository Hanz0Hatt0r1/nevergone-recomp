# HPData in-memory ABI probe

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This probe is a prerequisite for later `EnemyActionsData::loadWBGFile` work. It validates the small, filesystem-independent `HPData` memory object and `HPRange` call ABI before any real WBG input is used.

## Proven static boundary

Thumb disassembly shows:

- `HPData::HPData(unsigned char const*, unsigned long)` at instruction offset `0x2c64ac`;
- `HPData::~HPData()` at `0x2c6470`;
- `getBytes(char*, HPRange)` at `0x2c6568`;
- `getBytes(int&, HPRange)` at `0x2c658a`;
- `getBytes(unsigned int&, HPRange)` at `0x2c65ac`;
- `getBytes(float&, HPRange)` at `0x2c65ce`;
- `getBytes(bool&, HPRange)` at `0x2c65f0`.

The constructor initializes the inherited `CCObject`, allocates `source_size + 1`, copies the supplied bytes, stores the byte length at `this+0x14`, stores the owned buffer pointer at `this+0x18`, and writes a trailing NUL. The object therefore needs `0x1c` bytes on ARM32. The destructor frees the owned buffer and clears both `+0x14` and `+0x18`.

The five `getBytes` wrappers all compute `owned_buffer + HPRange.start` and copy `HPRange.length` bytes. On this ARM32 ABI the two-word `HPRange` value follows `this` and the destination pointer in registers `r2`/`r3` for these signatures.

## Dynamic probe input

`HpDataMemoryProbe` constructs one guarded `0x1c`-byte object from 16 controlled bytes. It then checks:

- constructor-owned copy and trailing NUL;
- int range `[0,4)` -> `0x78563412`;
- float range `[4,8)` -> raw IEEE-754 bits `0x3fc00000` (`1.5f`);
- unsigned range `[8,12)` -> `0x89abcdef`;
- bool range `[12,13)` -> true;
- char range `[13,16)` -> ASCII `WBG`;
- object guard preservation;
- destructor clearing of length and owned-buffer slots.

The executable probe calls only the memory constructor, five exported readers and the destructor. It does not call `HPData::createWithContentsOfFile`, `CCFileUtils`, `EnemyActionsData`, or the WBG parser.

## Why not use `createWithContentsOfFile` yet

`HPData::createWithContentsOfFile(char const*)` immediately enters `CCFileUtils::sharedFileUtils()` and a virtual file-data lookup. That inherits the Android FileUtils/ZipFile bootstrap requirements documented in [`enemy-actions-load-wbg-boundary.md`](enemy-actions-load-wbg-boundary.md). Keeping this probe memory-only isolates the `HPData` and `HPRange` ABI from that environment.

The repository CI validates the exact allowed native-call surface statically. Actual native execution is intended for the locally provisioned unidbg environment described in `tools/unidbg/README.md`.