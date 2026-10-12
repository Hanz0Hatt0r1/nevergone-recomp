# Direct GameScene sprite search-path resolution (2026-10-12)

Target: clean-room reconstruction of Never Gone 1.0.9 resource loading.

The reconstructed atlas loader already follows the recovered `AppDelegate::AddAllSearchPath()` order: writable imported root first, then the 59 recovered child search roots with first-match semantics.

The direct type-0 path still diverged in two connected places. The native render queue rewrote every ordinary sprite basename to `gamescene/gs_res_image_file/<name>`, and Java staging then resolved that rewritten path only relative to the writable imported root. The original path instead passes the logical basename to `CCSprite::create(file)`, after which `CCFileUtils::fullPathForFilename()` applies the registered search paths.

The reconstruction now preserves that split:

1. the native queue validates and retains the logical direct-sprite basename;
2. Java validates the logical resource name again;
3. staging tries the writable imported root;
4. staging tries each recovered child search root in original order;
5. the canonical result must remain inside the user-owned imported asset tree;
6. the first file match is decoded and staged.

This also preserves the recovered `gkanniu2.png -> empty.png` substitution: the resulting `empty.png` basename resolves from the writable root before any child search directory.

Atlas frame names are not changed by this rule; only `KIND_DIRECT_FILE` requests use the direct resource lookup.

Synthetic smoke coverage verifies logical-basename retention in the native queue, a basename present only under `controls_ui`, root-first precedence for duplicates, missing-resource failure, and rejection of traversal/absolute names.
