#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools/ml/run_nevergone_beaten_script_engine_manager_v39.sh"
text = RUNNER.read_text(encoding="utf-8")

symbol_call = (
    'ExportAccessorReturnConsumers.java \\\n'
    '  "cocos2d::CCScriptEngineManager::sharedManager" "$CONSUMERS"'
)
legacy_address_call = (
    'ExportAccessorReturnConsumers.java \\\n'
    '  00536cdc "$CONSUMERS"'
)

assert symbol_call in text, "v39 must resolve sharedManager by exact analyzed symbol"
assert legacy_address_call not in text, "v39 must not assume packaged ELF VA equals Ghidra project VA"

print("v39 sharedManager symbol-resolution smoke: PASS")
