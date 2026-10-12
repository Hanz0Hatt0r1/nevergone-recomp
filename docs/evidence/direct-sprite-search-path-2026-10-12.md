# Direct GameScene sprite search-path resolution (2026-10-12)

Target: clean-room reconstruction of Never Gone 1.0.9 resource loading.

The reconstructed atlas loader already follows the recovered `AppDelegate::AddAllSearchPath()` order: writable imported root first, then the 59 recovered child search roots with first-match semantics.

The direct `CCSprite::create(file)` staging path still resolved `file` only relative to the writable imported root. That was stricter than the original Cocos2d-x lookup and could reject a valid direct sprite that exists under a registered child search path such as `gamescene`, `controls_ui`, or `other`.

The direct staging path now reuses the same bounded Cocos resource resolver as atlas lookup:

1. validate the logical resource name;
2. try the writable imported root;
3. try each recovered child search root in original order;
4. require the canonical result to remain inside the user-owned imported asset tree;
5. return the first file match.

Atlas frame names are not changed by this rule; only `KIND_DIRECT_FILE` requests use the direct resource lookup.

Synthetic smoke coverage verifies a basename present only under `controls_ui`, root-first precedence for duplicates, missing-resource failure, and rejection of traversal/absolute names.
