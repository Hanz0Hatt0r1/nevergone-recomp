#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile

MODULE_PATH = Path(__file__).with_name("android_runtime_diagnostics.py")
spec = importlib.util.spec_from_file_location("android_runtime_diagnostics", MODULE_PATH)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


def write_xml(path: Path, body: str) -> None:
    path.write_text(
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
        '<hierarchy rotation="0">' + body + '</hierarchy>',
        encoding="utf-8",
    )


def expect_value_error(action) -> None:
    try:
        action()
    except ValueError:
        return
    raise AssertionError("expected ValueError")


def main() -> int:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        xml = root / "window.xml"

        write_xml(
            xml,
            '<node scrollable="true" class="android.widget.ScrollView" bounds="[20,800][1060,1800]">'
            '<node text="Refresh runtime diagnostics" bounds="[100,900][500,980]" />'
            '<node text="Never Gone Recomp&#10;app lifecycle: resumed&#10;'
            'resume events: 3&#10;pause events: 2&#10;GL surface generation: 4&#10;" '
            'bounds="[0,1000][1080,1700]" />'
            '</node>',
        )
        assert module.refresh_center(xml) == (300, 940)
        assert module.scroll_swipe(xml) == (540, 1550, 540, 1050)
        assert module.scroll_swipe(xml, "toward-end") == (540, 1550, 540, 1050)
        assert module.scroll_swipe(xml, "toward-start") == (540, 1050, 540, 1550)
        counters = module.renderer_counters(xml)
        assert counters.resume == 3
        assert counters.pause == 2
        assert counters.surface == 4

        write_xml(
            xml,
            '<node content-desc="Refresh runtime diagnostics" bounds="[100,200][500,280]" />',
        )
        assert module.refresh_center(xml) == (300, 240)

        # Android Buttons commonly expose textAllCaps in uiautomator even when
        # the Java source label uses title case. Treat presentation casing as
        # irrelevant without weakening the exactly-one-control requirement.
        write_xml(
            xml,
            '<node text="REFRESH RUNTIME DIAGNOSTICS" bounds="[32,916][1888,1042]" />',
        )
        assert module.refresh_center(xml) == (960, 979)

        write_xml(
            xml,
            '<node text="  Refresh   runtime diagnostics  " bounds="[100,200][500,280]" />',
        )
        assert module.refresh_center(xml) == (300, 240)

        # When imported game files are ready MainActivity collapses the
        # diagnostics ScrollView and exposes this control instead. The device
        # harness may expand it, but must not match the expanded-state
        # "Hide developer tools" label and accidentally collapse the panel.
        write_xml(
            xml,
            '<node text="Developer tools" bounds="[700,1400][1060,1520]" />',
        )
        assert module.developer_tools_center(xml) == (880, 1460)
        expect_value_error(lambda: module.refresh_center(xml))

        write_xml(
            xml,
            '<node text="DEVELOPER TOOLS" bounds="[700,1400][1060,1520]" />',
        )
        assert module.developer_tools_center(xml) == (880, 1460)

        write_xml(
            xml,
            '<node text="Hide developer tools" bounds="[700,1400][1060,1520]" />',
        )
        expect_value_error(lambda: module.developer_tools_center(xml))

        write_xml(
            xml,
            '<node text="Refresh runtime diagnostics" bounds="[10,10][10,20]" />',
        )
        expect_value_error(lambda: module.refresh_center(xml))

        write_xml(
            xml,
            '<node text="Refresh runtime diagnostics" bounds="[0,0][100,100]" />'
            '<node text="REFRESH RUNTIME DIAGNOSTICS" bounds="[100,0][200,100]" />',
        )
        expect_value_error(lambda: module.refresh_center(xml))

        # Android can expose a real ScrollView as scrollable=false when the
        # accessibility snapshot is clipped to the current viewport.
        write_xml(
            xml,
            '<node scrollable="false" class="android.widget.ScrollView" '
            'bounds="[20,800][1060,1800]" />',
        )
        assert module.scroll_swipe(xml) == (540, 1550, 540, 1050)
        assert module.scroll_swipe(xml, "toward-start") == (540, 1050, 540, 1550)

        # An explicit accessibility scroll target wins over a non-scrollable
        # ScrollView fallback, keeping the selection deterministic.
        write_xml(
            xml,
            '<node scrollable="true" class="android.view.ViewGroup" bounds="[0,0][800,1000]" />'
            '<node scrollable="false" class="android.widget.ScrollView" bounds="[0,1000][800,1600]" />',
        )
        assert module.scroll_swipe(xml) == (400, 750, 400, 250)
        assert module.scroll_swipe(xml, "toward-start") == (400, 250, 400, 750)

        expect_value_error(lambda: module.scroll_swipe(xml, "sideways"))

        write_xml(xml, '<node scrollable="false" bounds="[0,0][100,100]" />')
        expect_value_error(lambda: module.scroll_swipe(xml))

        write_xml(
            xml,
            '<node scrollable="false" class="android.widget.ScrollView" bounds="[0,0][100,100]" />'
            '<node scrollable="false" class="android.widget.ScrollView" bounds="[0,100][100,200]" />',
        )
        expect_value_error(lambda: module.scroll_swipe(xml))

        write_xml(
            xml,
            '<node scrollable="true" bounds="[0,0][100,100]" />'
            '<node scrollable="true" bounds="[0,100][100,200]" />',
        )
        expect_value_error(lambda: module.scroll_swipe(xml))

        write_xml(
            xml,
            '<node scrollable="true" bounds="[0,0][100,2]" />',
        )
        expect_value_error(lambda: module.scroll_swipe(xml))

        write_xml(
            xml,
            '<node text="resume events: 2 pause events: 1" bounds="[0,0][100,100]" />',
        )
        expect_value_error(lambda: module.renderer_counters(xml))

        write_xml(
            xml,
            '<node text="resume events: 2 pause events: 1 GL surface generation: 3" '
            'bounds="[0,0][100,100]" />'
            '<node text="resume events: 4 pause events: 3 GL surface generation: 5" '
            'bounds="[0,100][100,200]" />',
        )
        expect_value_error(lambda: module.renderer_counters(xml))

        write_xml(xml, '<node text="unrelated" bounds="[0,0][100,100]" />')
        expect_value_error(lambda: module.renderer_counters(xml))

    print("Android runtime diagnostics parser smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
