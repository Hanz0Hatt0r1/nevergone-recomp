# MeActionsSystem player RGBA4444 evidence (2026-10-12)

Target: clean-room reconstruction of the first player action-atlas texture path in Never Gone 1.0.9. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

No original binary or proprietary texture/atlas data is stored in this repository. The notes below retain only bounded behavioral observations from the user-owned ARMv7 library.

## MeActionsSystem establishes a temporary format-6 scope

`MeActionsSystem::initWithFile(...)` is the exported Thumb function at symbol `0x0032bd11`.

At aligned code address `0x0032bf98` it executes:

```text
movs r0, #6
bl   CCTexture2D::setDefaultAlphaPixelFormat
```

Immediately after this, the function resolves the first `ActionFrameData` entry from the array at `MeActionsSystem + 0x110`. The initial frame name is read from `ActionFrameData + 0x68`.

If that frame is not already in `CCSpriteFrameCache`, the fallback at `0x0032c02c` reads the action atlas plist from `ActionFrameData + 0x60` and calls `CCSpriteFrameCache::addSpriteFramesWithFile(...)` at `0x0032c038`. Therefore the player action atlas fallback load executes while the global default alpha pixel format is value `6`.

Near the completion of the same init path, `0x0032c6fa..0x0032c6fc` executes:

```text
movs r0, #0
bl   CCTexture2D::setDefaultAlphaPixelFormat
```

So the format-6 setting is intentionally scoped to player-action initialization and then restored to format `0`; it should not be applied globally to ordinary GameScene resources.

## Pixel-format value 6 is RGBA4444

`CCTexture2D::initPremultipliedATextureWithImage(...)` is exported at Thumb symbol `0x00543765`.

The branch at aligned address `0x0054383c` compares the selected texture pixel format with `6`. Its conversion loop at `0x00543854..0x00543872` reads one already-premultiplied 32-bit RGBA pixel, keeps the high four bits of each component, and writes one 16-bit value in this layout:

```text
RRRR GGGG BBBB AAAA
```

The format dispatch in `CCTexture2D::initWithData(...)` maps pixel-format value `6` to:

```text
format = GL_RGBA
 type   = GL_UNSIGNED_SHORT_4_4_4_4 (0x8033)
```

By contrast, restored pixel-format value `0` dispatches to the normal `GL_RGBA` + `GL_UNSIGNED_BYTE` path.

## Ordering with PNG premultiplication

The PNG decoder evidence in `png-premultiply-blend-2026-10-12.md` shows that four-channel PNG bytes are first premultiplied at 8-bit precision with:

```text
channel * (alpha + 1) >> 8
```

The format-6 conversion then truncates those premultiplied R/G/B bytes and the original alpha byte to their high nibbles. The clean-room uploader therefore preserves this order:

1. Android `Bitmap.getPixels()` ARGB input;
2. recovered 8-bit premultiplication;
3. high-nibble RGBA4444 packing;
4. GLES2 upload with `GL_UNSIGNED_SHORT_4_4_4_4`;
5. premultiplied sprite blending (`GL_ONE`, `GL_ONE_MINUS_SRC_ALPHA`).

## Clean-room scope

The project-owned request model already identifies player-body sprite-frame requests separately from ordinary scene atlas/direct-file requests. Only those player-body uploads are tagged RGBA4444. Ordinary scene assets remain RGBA8888.

This is a bounded reconstruction of the evidenced `MeActionsSystem::initWithFile` player-action path. It does not claim that every player-related texture in every original code path necessarily used RGBA4444.
