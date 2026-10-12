# PNG premultiply and sprite blend evidence (2026-10-12)

Target: clean-room reconstruction of Never Gone 1.0.9 ARMv7 rendering behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

No original binary, image, atlas, or other proprietary game resource is stored in this repository. The observations below are derived from bounded disassembly of the user-owned original library and are recorded only as behavioral metadata needed by the project-owned renderer.

## Original PNG decode path

The shipped dynamic symbol table identifies:

- `cocos2d::CCImage::initWithImageData(...)` at Thumb symbol `0x00535339`;
- `cocos2d::CCImage::_initWithPngData(...)` at aligned address `0x00534f9c`;
- `cocos2d::CCTexture2D::initWithImage(CCImage*)` at Thumb symbol `0x00543955`;
- `cocos2d::CCTexture2D::hasPremultipliedAlpha()` at Thumb symbol `0x005435bb`;
- `cocos2d::CCSprite::updateBlendFunc()` at Thumb symbol `0x00539429`.

`CCImage::initWithImageData()` recognizes the PNG signature and branches directly to `_initWithPngData()`.

For decoded PNG rows with four bytes per pixel, `_initWithPngData()` walks each RGBA pixel and transforms each color channel with the exact integer expression:

```text
premultiplied_channel = channel * (alpha + 1) >> 8
```

Alpha itself is retained unchanged. After the loop the function writes `1` to the `CCImage` premultiplied-alpha byte at object offset `+0x21`. The same path sets the alpha-present byte at `+0x20` for four-channel PNG data.

The use of `alpha + 1` is observable directly in the instruction sequence around `0x00535148..0x00535172`; it is not an approximation added by the clean-room runtime.

## CCImage -> CCTexture2D propagation

`CCTexture2D::initPremultipliedATextureWithImage()` copies the image premultiplied-alpha byte from `CCImage +0x21` into the texture-owned flag at `CCTexture2D +0x38` after successful image conversion/upload preparation.

This makes the blend choice texture-data-driven in the shipped engine rather than a TexturePacker metadata decision.

## Sprite blend selection

`CCSprite::updateBlendFunc()` reads the texture at `CCSprite +0x124` and calls `CCTexture2D::hasPremultipliedAlpha()`.

The two recovered branches are:

```text
premultiplied texture:
    src = GL_ONE
    dst = GL_ONE_MINUS_SRC_ALPHA
    opacityModifyRGB = true

non-premultiplied texture:
    src = GL_SRC_ALPHA
    dst = GL_ONE_MINUS_SRC_ALPHA
    opacityModifyRGB = false
```

For the ordinary PNG RGBA path above, the texture is therefore rendered with `GL_ONE, GL_ONE_MINUS_SRC_ALPHA` after the explicit integer premultiply pass.

## Clean-room consequence

Android `Bitmap.getPixels()` exposes non-premultiplied ARGB color integers even when the Bitmap backing storage is premultiplied. The project-owned GameScene staging path passes those ARGB integers to native texture upload.

To reproduce the original engine boundary deterministically, the project-owned GLES upload now applies the recovered `channel * (alpha + 1) >> 8` transform when converting ARGB to RGBA, and the GameScene renderer uses `GL_ONE, GL_ONE_MINUS_SRC_ALPHA` for those uploaded textures.

Opaque pixels are unchanged because `alpha = 255` produces `channel * 256 >> 8 == channel`.

This evidence covers the current PNG-backed GameScene sprite path. It does not claim that every non-PNG texture format in the original runtime uses premultiplied alpha.
