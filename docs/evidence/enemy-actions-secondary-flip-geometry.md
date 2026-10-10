# EnemyActions secondary `+0x16a` geometry

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native range

The bounded branch is `0x2ac986..0x2aca6a`. It is entered only when system byte `+0x16a` is nonzero, after the secondary base-sprite slice has already set position, rotation, anchor and `setFlipY(true)` on sprite `system+0x114`.

## Point helpers

Two local helper bodies are directly recoverable:

- `0x2ab232` computes point addition `(a.x+b.x, a.y+b.y)`;
- `0x2ab25e` computes point subtraction `(a.x-b.x, a.y-b.y)`.

Native first builds pivot `(primary AFD+0x1c, primary AFD+0x20)`, obtains the current secondary-sprite position, computes `pivot - position`, and negates only the resulting Y component. Adding the pivot again therefore mirrors X while preserving the current Y.

It then adds system point `(+0x194,+0x198)` and stores that intermediate point into `system+0x1c8`.

Immediately before `setPosition()`, native recomputes the same Y formula used by the preceding base secondary slice and writes it to `system+0x1cc`. Consequently the final position is:

- X: native-order mirror result plus `system+0x194`;
- Y: unchanged from the base secondary plan.

For a base position `(x,y)` and primary pivot X `p`, the X result is algebraically `2*p - x + system+0x194`, but the project helper preserves the subtraction/addition order used by native.

## Anchor and rotation

Anchor X is calculated as `0.5 - (primary AFD+0x24 - 0.5)`; anchor Y remains primary `AFD+0x28`.

For rotation, native loads the raw 32-bit word at primary `AFD+0x34` and executes `EOR #0x80000000` before calling `setRotation()`. This is an IEEE-754 sign-bit toggle, not an arithmetic floating-point negate. Signed zero and NaN payloads therefore retain their non-sign bits exactly.

## Project-owned reconstruction

`enemy_actions_secondary_flip_geometry.h` consumes the already reconstructed secondary base plan plus the primary frame and offset-level system inputs. It returns only the fields rewritten by this branch and performs no cocos2d calls.

The armor-array processing beginning at `0x2aca6c` remains outside this contract.

No proprietary payload or decompiler-derived source is included.
