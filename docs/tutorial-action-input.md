# Tutorial action-input boundary

## Purpose

The reconstructed tutorial already has semantic gameplay outcomes for Run, Jump, Attack, Block, Counter-attack, Dodge, Air Attack, Rage fill, and Execution. Only Run had a live input bridge. This boundary adds a project-owned A/B/C action-input layer without inventing screen-space hitboxes that are not yet recovered.

## Evidence-backed behavior

Frozen original-game captures establish these prompts:

- Jump: `Tap C to jump.`
- Attack: `Tap A to attack. Eliminate all the enemies in front of you.`
- Block: `Tap and hold B to [Block] the enemy attack.`
- Counter-attack: attack with A immediately after a successful block.
- Dodge: tap left/right while holding B.
- Air Attack: jump with C, then attack with A in the air.

The action-input contract therefore names the original semantic controls A/B/C and gesture classes press/release/tap/hold. It deliberately does not claim any screen rectangle, radius, z-order, or touch priority.

## Tutorial bridge

A semantic `C + tap` advances the tutorial only when both conditions are true:

1. the tutorial session is already in the interactive phase; and
2. the gameplay tutorial step is `Jump`.

This publishes `kJumpPerformed`, advancing `Jump -> Attack`.

Other A/B/C inputs are recorded but do not directly advance combat lessons. Attack waits for the tutorial enemy objective to be defeated; Block waits for a successful blocked hit; Counter-attack and Dodge wait for their corresponding gameplay outcomes; Air Attack waits for the bat objective. This prevents raw button presses from skipping evidence-backed objectives.

## Next boundary

The remaining missing layer is the original `Controls` A/B/C hit-test/dispatch geometry. Once recovered, it should map Android touches to these semantic controls and call `game_scene_action_input::observe(...)`. No tutorial state-machine change should be required.
