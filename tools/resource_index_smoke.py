#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import tempfile
from pathlib import Path

from resource_index import build_index


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary_directory:
        root = Path(temporary_directory) / "assets"
        (root / "gamescene" / "gs_list").mkdir(parents=True)
        (root / "player").mkdir(parents=True)
        (root / "gamescene" / "gs_list" / "scene.glData").write_bytes(b"scene")
        (root / "player" / "hero.bin").write_bytes(b"hero-data")

        index = build_index(root)
        assert index["schema_version"] == 1
        assert index["summary"]["files"] == 2
        assert index["summary"]["bytes"] == len(b"scene") + len(b"hero-data")
        assert index["summary"]["subsystems"] == {"player": 1, "scene": 1}
        assert index["summary"]["extensions"] == {".bin": 1, ".gldata": 1}
        assert [entry["path"] for entry in index["entries"]] == [
            "gamescene/gs_list/scene.glData",
            "player/hero.bin",
        ]
        assert index["entries"][0]["sha256"] == hashlib.sha256(b"scene").hexdigest()

        hashed = build_index(root, path_mode="hash", content_hash=False)
        expected_path_hash = hashlib.sha256(
            b"gamescene/gs_list/scene.glData"
        ).hexdigest()
        assert hashed["entries"][0]["path"] == expected_path_hash
        assert hashed["entries"][0]["sha256"] == ""
        assert hashed["content_hash"] == "none"

    print("resource_index smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
