# Manual reconstruction: enemy03 AI decision

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function and result codes

`enemy03AICPP::enemy03_AI` is at Ghidra/RevEngInstr image address
`0x002baf40` and ELF address `0x002aaf40`. Its decompiled return type is
incorrectly shown as `bool`: `EnemyObject::eventAction(float)` compares the
result with integers 1 through 6. The clean-room contract therefore returns
the recovered integer code and preserves all seven possible results.

The function evaluates one candidate action state at a time. Its direct
inputs, in native parameter order after `this`, are the precomputed integer
random gate, candidate state, current enemy state, horizontal position delta,
relative-facing flag, one unused integer, enemy timer, candidate action range
maximum, candidate action range minimum, and player state. The callsite also
passes `EnemyObject +0x248` after those values, but the recovered body does not
read that extra call argument.

The exact selection rules are implemented in
`android/app/src/main/cpp/enemy03_ai_contract.cpp`:

- current state `0x1c` always returns 0;
- current state `0x2f` only considers candidate state `0x1c`, returning 1
  below horizontal delta 300;
- with random gate below 8000, player states 2/`0x15` can select 43 (code 6)
  or 36 (code 1); player states `0x19`, `0x1a`, and `0x20` can select 38
  (code 1); other player states can select 38, 37, 2, or 3 when the horizontal
  delta is inside the candidate's min/max range and the enemy timer exceeds
  200;
- current states below 2 or equal to `0x14` can select candidate 27 below
  horizontal delta 280, returning code 5, 3, or 4 at random-gate thresholds
  5000 and 7000;
- candidate 1 returns code 2 at horizontal delta 301 or greater, otherwise 0.

`eventAction` forms the random gate from `lrand48()` with literal-pool
constants `0x30000000` (`2^-31`) and `0x461c4000` (`10000.0f`):
`int(float(lrand48_value) * 2^-31 * 10000.0f)`. The contract exposes this
single-precision conversion so the decision evaluator receives the same
integer as native code. The random draw itself is not scheduled by the
clean-room actor runtime.

The recovered `eventAction` caller consumes code 1 by returning the candidate
state. Codes 2 through 6 select separate movement/transition branches. The
contract intentionally leaves those caller branches outside this function.
The same contract now models the ordered candidate scan: native calls
`checkInputStateIsBeaten()` first, evaluates only indices for which it returns
false, and uses the first nonzero `enemy03_AI` result. It also accepts the
lesson-step-6/no-progress override, which clears results for candidates other
than `0x1c`. The one-second accumulator consumes one second per invocation,
even if the accumulated total remains above one.

## Input provenance

At image address `0x002c1b28`, `eventAction` passes the candidate action-array
index, current `EnemyObject +0x274` state, and the absolute X difference between
the chosen enemy and player target points. The facing flag is derived from the
target direction and `EnemyObject +0x27c`. The player state is
`MEPlayer::actionState()`, except that a recent-lansquenet path substitutes
state `0x1b`.

The candidate action's range maximum and minimum are the integer conversions of
`EnemyActionsData +0xd4` and `+0xd0`. `EnemyActionsSystem::initWithFile()` copies
these to `EnemyActionsSystem +0x2c0` and `+0x2c4`; its getters expose those
fields. The timer is `EnemyObject +0x2c8`: `initWithID()` initializes it to
1000, `enemyPhysical(int)` adds a delta and clamps it to `[0, 1000]`, and
`enemy03_Phy(state, timer)` subtracts 200 for states 2/3 or 300 for states
`0x25`, `0x26`, and `0x28`, then clamps negative results to zero. The helpers
for the two timer operations are implemented in the same contract.

## Remaining boundary

The decision function, random scaling, candidate filtering, one-second gate,
and timer transforms are now executable contracts, but the tutorial actor
renderer/live gate does not yet execute the full native `eventAction` loop.
The `lrand48()` draw scheduling and codes 2 through 6's movement branches
still need to be connected before this contract can drive a live enemy. No
movement result is inferred here.
