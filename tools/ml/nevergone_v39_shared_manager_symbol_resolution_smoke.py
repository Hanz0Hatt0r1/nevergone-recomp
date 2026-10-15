#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools/ml/run_nevergone_beaten_script_engine_manager_v39.sh"
text = RUNNER.read_text(encoding="utf-8")

assert '"cocos2d::CCScriptEngineManager::sharedManager" "$CONSUMERS"' in text, (
    "v39 must resolve sharedManager by exact analyzed symbol"
)
assert '00536cdc "$CONSUMERS"' not in text, (
    "v39 must not assume packaged ELF VA equals Ghidra project VA"
)

print("v39 sharedManager symbol-resolution smoke: PASS")
