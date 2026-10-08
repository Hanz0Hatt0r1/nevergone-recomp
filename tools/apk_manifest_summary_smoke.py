#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

TOOL = Path(__file__).with_name("apk_manifest_summary.py")
spec = importlib.util.spec_from_file_location("apk_manifest_summary", TOOL)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

XML = b'''<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="example.game" android:versionCode="7" android:versionName="1.2.3">
  <uses-sdk android:minSdkVersion="9" android:targetSdkVersion="17" />
  <uses-feature android:glEsVersion="0x00020000" />
  <uses-permission android:name="android.permission.INTERNET" />
  <application android:label="Example">
    <activity android:name=".MainActivity">
      <intent-filter>
        <action android:name="android.intent.action.MAIN" />
        <category android:name="android.intent.category.LAUNCHER" />
      </intent-filter>
    </activity>
  </application>
</manifest>
'''

root = module.parse_manifest(XML)
summary = module.summarize(root)
assert summary["package"] == "example.game"
assert summary["versionCode"] == "7"
assert summary["versionName"] == "1.2.3"
assert summary["sdk"] == {"minSdkVersion": "9", "targetSdkVersion": "17"}
assert summary["features"][0]["glEsVersionDecoded"] == "2.0"
assert summary["permissions"] == ["android.permission.INTERNET"]
assert summary["launcherActivities"] == ["example.game.MainActivity"]
assert summary["components"]["activities"][0]["launcher"] is True
print("apk manifest summary smoke: ok")
