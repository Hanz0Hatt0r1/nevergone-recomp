# SceneActions spatial-trigger evidence — 2026-10-13

## Scope

This note records bounded behavior recovered from the verified Never Gone 1.0.9 ARMv7 library (`SHA-256 94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`). No original binary, disassembly listing, decoded asset, or decompiler output is committed.

Relevant retained dynamic symbols/code boundaries are:

- `GameSceneClassLayer::showEventObjTip(...)` — Thumb code `0x0034b048`;
- `GameSceneObject::triggerSceneAction()` — `0x0034e1b8`;
- `GameSceneObject::isContainsRect(CCRect)` — `0x0034e268`;
- `GameSceneObject::getObjectRect()` — `0x0034e4a6`;
- cocos2d `CCRect::intersectsRect(...)` — `0x00517aae`.

## Event branch

`showEventObjTip` has a dedicated unsigned range check equivalent to event IDs `2001..2019` inclusive.

For that range the routine:

1. scans the layer object array in order for the **first** `GameSceneObject` whose retained integer at `+0x108` equals the requested event ID;
2. obtains that source object's `getObjectRect()`;
3. scans every layer object in array order;
4. calls `isContainsRect(source_rect)` on each candidate;
5. when it returns true, calls that candidate's `triggerSceneAction()`.

If no source object matches the event ID, this branch produces no SceneAction trigger calls.

The timed `GameScene::TriggerSceneAction(float)` caller emits only IDs `2001..2008`, but `showEventObjTip` itself accepts the wider `2001..2019` branch.

## getObjectRect

When `GameSceneObject +0x10c` has no sprite, the original constructs and returns exactly:

```text
CCRect(0, 0, 1, 1)
```

When a sprite exists:

1. call `CCSprite::sprBoundingBox()`;
2. read the owning `GameSceneObject` node's `getPositionX()`;
3. add that X value to the returned rectangle's `origin.x`;
4. leave Y, width and height unchanged.

## isContainsRect

The candidate returns false immediately when:

- `GameSceneObject +0x10c` is null;
- serialized resource name is `czyanwu1.png`;
- serialized resource name is `czyanwu3.png`.

For other objects it computes the same sprite rectangle as above — `sprBoundingBox()` plus the GameSceneObject position-X adjustment — and calls cocos2d `CCRect::intersectsRect` with the source rectangle.

Several recovered light-resource names perform fade/visibility side effects after a successful intersection. Those paths still return true. They are intentionally excluded from the pure spatial predicate because they do not change which objects receive `triggerSceneAction()`.

## Rectangle edge policy

The shipped cocos2d `CCRect::intersectsRect` returns false only when one rectangle's maximum coordinate is **strictly less than** the other's minimum coordinate on X or Y. Therefore edge touching (`max == min`) counts as intersection.

## klhuo fallback sprite

The exact `klhuo-1.png` `GameSceneObject::initWithData()` branch creates the born/idle `SceneActionsSystem` pair without an ordinary resource sprite, then joins common object initialization. The common tail observes `+0x10c == nullptr`, creates `empty.png`, stores it at `+0x10c`, and makes it invisible.

Applying the already recovered project asset transform to the original `assets/empty.png` proves it is a valid **20 x 20** PNG. Only this metadata is recorded here; decoded/original asset bytes remain local and uncommitted.

This explains how the klhuo object can participate in `isContainsRect()` even though its visible content is provided by SceneActionsSystem children rather than the fallback sprite.

## Project reconstruction

`scene_actions_spatial_trigger.h` provides a renderer-independent contract over:

- retained event ID;
- serialized resource name;
- GameSceneObject position X;
- the already-evaluated `sprBoundingBox()` rectangle or absence of a sprite.

It reconstructs first-source selection, the exact `getObjectRect` fallback, fog exclusions, edge-inclusive intersection, ordered candidate scan, and the list of objects whose native `triggerSceneAction()` invocation is reached.

It deliberately does not yet:

- derive `sprBoundingBox()` from serialized transforms/assets;
- map the serialized object field that becomes native `+0x108`;
- perform decorative fade side effects;
- call the live SceneActions runtime;
- select a PRNG variant.

Those remain separate integration boundaries so the spatial predicate can be tested without inventing missing object-layout semantics.
