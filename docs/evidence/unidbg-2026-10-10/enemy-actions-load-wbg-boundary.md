# EnemyActionsData `loadWBGFile` dependency boundary

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the bounded prologue of `EnemyActionsData::loadWBGFile(cocos2d::CCString*)` at instruction offset `0x28f430`. It exists to define what must be initialized before a future dynamic WBG probe. No WBG serialization fields are inferred here.

## Pre-HPData call sequence

Before the function knows whether the requested file exists, it performs this call sequence:

1. `0x28f44a` -> `AppParameters::sharedAppParameters()` (`0x2938bc`).
2. `0x28f44e` -> `cocos2d::CCFileUtils::sharedFileUtils()` (`0x534c30`).
3. `0x28f45c` -> `cocos2d::CCString::getCString()` (`0x519e8e`) on the input name.
4. `0x28f466` -> virtual `CCFileUtils::fullPathForFilename(char const*)` (`0x532da4`).
5. `0x28f46a` -> `CCString::create(std::string const&)` (`0x519f8a`) for the resolved path.
6. `0x28f478` -> `CCString::getCString()` (`0x519e8e`) on that resolved path.
7. `0x28f47c` -> `HPData::createWithContentsOfFile(char const*)` (`0x2c652c`).

The virtual target in step 4 is no longer ambiguous. `sharedFileUtils()` constructs a `CCFileUtilsAndroid`. Its vtable symbol is `0x926098`, so the Itanium ABI address point is `0x9260a0`. The `+0x18` slot loaded by `loadWBGFile` is at `0x9260b8` and contains Thumb symbol value `0x532da5`, i.e. instruction offset `0x532da4`, exported as `CCFileUtils::fullPathForFilename(char const*)`.

## Why a direct unidbg call is not yet a leaf probe

Both singleton calls can perform first-use initialization.

`AppParameters::sharedAppParameters()` allocates `0x434` bytes when the singleton is absent, invokes the constructor at `0x293654`, then dispatches vtable slot `+0x24`. The `AppParameters` vtable proves that slot contains Thumb value `0x291bb9`, `AppParameters::init()` at instruction offset `0x291bb8`.

`CCFileUtils::sharedFileUtils()` allocates `0x3c` bytes for `CCFileUtilsAndroid`, invokes the constructor at `0x534c14`, then dispatches vtable slot `+0x78`. The Android file-utils vtable contains Thumb value `0x534ca9` there, `CCFileUtilsAndroid::init()` at `0x534ca8`. The same singleton bootstrap then calls `getApkPath()` (`0x535c88`) and constructs a `ZipFile` (`0x5410e4`).

Therefore a future missing-file probe must not treat `loadWBGFile` as a self-contained parser call. It needs a validated AppParameters/FileUtils environment or already-initialized singleton state.

## Proven null-file branch

After `HPData::createWithContentsOfFile`, `loadWBGFile` compares the returned pointer with zero at `0x28f484`. The branch at `0x28f486` jumps to `0x290422` when HPData is null. That destination performs stack-canary validation and reaches the function return at `0x290438`.

The first typed HPData reads exist only on the non-null path:

- `0x28f4a0` -> `HPData::getBytes(int&, HPRange)` (`0x2c658a`);
- `0x28f4b4` -> `HPData::getBytes(float&, HPRange)` (`0x2c65ce`) targeting `this+0xd4`;
- `0x28f4ca` -> the same float reader targeting `this+0xd0`;
- `0x28f4e2` -> `HPData::getBytes(unsigned int&, HPRange)` (`0x2c65ac`) targeting `this+0xc4`.

So a correctly initialized environment plus a guaranteed-missing file provides a bounded early-return path that does not enter WBG typed parsing. This remains a future dynamic probe target; the repository does not yet claim it has been executed successfully under unidbg.
