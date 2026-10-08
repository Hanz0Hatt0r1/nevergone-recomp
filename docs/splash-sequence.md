# Original splash sequence reconstruction

This document records clean-room behavioral evidence recovered from the user's original ARMv7 client. No original image bytes or decompiled source are stored in the repository.

## Source evidence

The original `libcocos2dcpp.so` exports `HelloWorld::FuncNEND6(CCNode*, void*)` at Thumb address `0x002c769d`. That routine references exactly six startup PNG names:

- `HIPPIEGOLO01.png`
- `HIPPIEGOLO02.png`
- `HIPPIEGOLO03.png`
- `HIPPIEGOLO04.png`
- `HIPPIEGOLO05.png`
- `HIPPIEGOLO06.png`

All six decoded files from the user's original APK are 562x572 paletted PNGs. Visual inspection shows 01 as the emblem, 02 as the HIPPIE GAME wordmark/base layer, and 03-06 as transient light/smoke overlays.

`FuncNEND6` constructs all of these sprites up front and runs independent Cocos2d-x actions on them. It is therefore not a six-frame equal-rate animation.

## Recovered timing

The same routine directly calls `CCDelayTime::create`, `CCFadeIn::create`, `CCFadeOut::create`, `CCSequence::create`, and `CCCallFuncND::create` with the following constants and ordering.

| Layer | Recovered action timeline |
| --- | --- |
| black startup layer | `FadeOut(3.0)` from opaque to transparent |
| HIPPIEGOLO01 | visible immediately; `Delay(2.0)`, `Delay(3.0)`, `FadeOut(0.5)`, remove |
| HIPPIEGOLO02 | visible immediately; `Delay(0.2)`, sound callback, `Delay(1.8)`, `Delay(3.0)`, `FadeOut(0.5)`, remove + `createUI()` |
| HIPPIEGOLO03 | starts at opacity 0; `Delay(1.0)`, `Delay(1.0)`, `FadeIn(0.5)`, `Delay(0.5)`, `FadeOut(1.0)`, remove |
| HIPPIEGOLO04 | starts at opacity 0; `Delay(1.2)`, `FadeIn(0.5)`, `FadeOut(0.7)`, remove |
| HIPPIEGOLO05 | starts at opacity 0; `Delay(1.5)`, `FadeIn(0.5)`, `FadeOut(0.7)`, remove |
| HIPPIEGOLO06 | starts at opacity 0; `Delay(1.8)`, `FadeIn(0.5)`, `FadeOut(0.7)`, remove |

The `HIPPIEGOLO02` sequence reaches `HelloWorld::FuncNEND2`, which removes the splash node, clears unused textures, and calls `HelloWorld::createUI()`. This establishes a splash-completion point of approximately 5.5 seconds after the sequence starts.

The recovered startup sound callback fires at approximately 0.2 seconds. Audio playback itself is not reconstructed by the current runtime yet.

## Runtime mapping

`android/app/src/main/cpp/splash_timeline.*` expresses this timing as pure project-owned state: a black-overlay alpha, six per-layer alpha values, and completion state. `sample_tick()` derives time from the already recovered 35 Hz game/update clock so the reconstructed timing remains independent from physical display refresh.

The renderer will consume this timeline after the six user-supplied textures are held simultaneously. Until then, the current single imported splash texture remains the visible fallback.
