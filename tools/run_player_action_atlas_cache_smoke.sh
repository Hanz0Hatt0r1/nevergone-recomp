#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/nevergone-player-action-atlas-cache-smoke"
rm -rf "$OUT"
mkdir -p "$OUT"

javac -d "$OUT" \
  "$ROOT/android/app/src/main/java/org/nevergone/recomp/ImportedPlistXml.java" \
  "$ROOT/android/app/src/main/java/org/nevergone/recomp/GameSceneAtlasListDiscovery.java" \
  "$ROOT/android/app/src/main/java/org/nevergone/recomp/GameSceneAtlasResolver.java" \
  "$ROOT/android/app/src/main/java/org/nevergone/recomp/PlayerActionAtlasCacheResolver.java" \
  "$ROOT/tools/PlayerActionAtlasCacheResolverSmoke.java"

java -ea -cp "$OUT" org.nevergone.recomp.PlayerActionAtlasCacheResolverSmoke

echo "Player action atlas cache resolver smoke: ok"
