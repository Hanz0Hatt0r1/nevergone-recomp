# SingleSelectHero: original-atlas character preview

## Verified source data

The original APK contains
`assets/gamescene_ui/SingleLogin_UI/SingleSelectHero/Singleselechero.plist/png`.
The existing `OriginalApkImporter` decodes the PNG to
`files/assets/gamescene_ui/SingleLogin_UI/SingleSelectHero/Singleselechero.png`
without flattening or inventing an OBB path.

The plist lists these **exact** preview body names:

| Hero | Normal body | Grey body | Untrimmed |
| --- | --- | --- | --- |
| Dawn / career 1 | `P01DL_Ani_0001.png` | `P01DL_Ani_0001_grey.png` | 218 x 465 |
| Higgs / career 2 | `P02DL_Ani_0001.png` | `P02DL_Ani_0001_grey.png` | 218 x 465 |

Higgs's upright source image is trimmed to 204 x 439 with offset
`{13,11}`; the plist records it as rotated in the 2048x2048 atlas.
Use `TexturePackerAtlasExtractor` rather than cropping or sampling
the rotated rectangle directly.

The atlas also contains `P01DL_Ani_0001b.png` and
`P02DL_Ani_0001b.png`. Their exact composition/animation timing has
**not** been recovered and they are not rendered as invented overlays.

## Implementation

`SingleSelectHeroBaseComposer` extracts and uploads four original
preview frames from **the same decoded selection atlas** already
used for `xrbeijing.png`, `HeroTable` and the five rune pairs.
They are retained as pixel backing in the native
`single_select_hero_compositor` and converted into GLES textures
only when the offline `opening-dialogue` scene is active.

The selected `single_select_hero_state` career chooses normal or grey
preview without keeping a second, renderer-only selection state.
The body is drawn after the background and before runes/information,
with aspect-fit transform and untrimmed center-anchor geometry.
The initial/settled preview anchor is now recovered as
**(287,320)** on the original 1136x640 design surface, not the
previous provisional (320,320):

- `SingleSelectHero::initUI` at `0x00438c20` constructs
  `xrshiguan.png` with X = `150 + 0.5 * contentWidth`
  and Y = `0.5 * visibleHeight` (machine instructions
  `0x00439344..0x0043938c`).
- In the original imported atlas, `xrshiguan.png` is
  274x514, making its X coordinate `150 + 137 = 287`.
- At `0x004393a0..0x004393ac` the original code copies that
  position into a `CCPoint`; at `0x00439646..0x00439662`
  it sets the `P01DL_Ani_0001.png` body to the same point.
- The five `xrguangai.png` carousel control points
  (`0x004393e0..0x00439454`) are spaced by that image's
  271-pixel content width. `Carousel()` computes the
  current-vs-first control-point displacement and counter-moves
  the hero container via `CCNode::setPositionX` at
  `0x00438ac8`. The selected body therefore returns to the
  original anchor on completion.

The original ELF SHA-256 is
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`,
and these instructions are from the user's
`00438c20.pcode-flow.tsv`, `004388ac.pcode-flow.tsv` and
`selection.instructions.jsonl.gz` exports. Only derived
facts are committed; no original executable or asset bytes.

**Remaining distinction:** the selected body *settled position* is
supported by this evidence. Intermediate door/carousel movement,
overlay effects and exact per-frame animation still require reconstruction.

A surface recreation invalidates GPU handles but keeps the decoded
CPU pixel backing. Explicit asset reload clears both CPU and GPU
preview frames before loading the possibly newer atlas.

This is **initial frame rendering**, not reconstruction of the full
animated selection scene, coffin/door timeline, grey transition, or
`_0001b` overlay behavior. The animation parity work remains open.

## Validation

- `tools/single_select_hero_character_layout_smoke.cpp` covers
  career selection, grey variant, shipped trim geometry, aspect-fit,
  invalid dimensions and out-of-range careers.
- `SingleSelectHeroBaseComposer.statusReport()` now includes
  `hero character frames retained: 4/4` and a separate failure string.
- The fast CI compiles/runs the host geometry regression.
- Android Java/NDK builds and an on-device side-by-side screenshot are
  still needed to establish successful integration and correct pose.

This change is intentionally independent of the offline fixed-name
and save deletion work in PR #738. It can be cherry-picked onto a local
branch without resetting working files.
