# Android manifest summary

This metadata-only summary is generated from the unmodified original Never Gone 1.0.9 APK with `tools/apk_manifest_summary.py`. No proprietary resources or binary manifest bytes are committed.

- APK SHA-256: `c8aaabfe28893998aaf891716147328245e16a043d95b7abdeac43ef6adc9cab`
- Package: `com.hippiegame.nevergone`
- Version: `1.0.9` (`versionCode 9`)
- minSdkVersion: `9`
- targetSdkVersion: `17`
- Platform build SDK: `19`
- Platform build version: `4.4.2-1456859`

## Launchers

- `com.hippiegame.nevergone.TJ_P_01`

## Features

- OpenGL ES `2.0` (encoded `0x00020000`)

## Permissions

- `android.permission.INTERNET`
- `android.permission.BATTERY_STATS`
- `android.permission.ACCESS_WIFI_STATE`
- `android.permission.ACCESS_NETWORK_STATE`
- `android.permission.CHANGE_NETWORK_STATE`
- `android.permission.CHANGE_WIFI_STATE`
- `android.permission.VIBRATE`
- `android.permission.BROADCAST_STICKY`
- `android.permission.READ_PHONE_STATE`
- `android.permission.SYSTEM_ALERT_WINDOW`
- `android.permission.RECEIVE_BOOT_COMPLETED`
- `android.permission.WAKE_LOCK`
- `android.permission.WRITE_EXTERNAL_STORAGE`
- `android.permission.READ_EXTERNAL_STORAGE`
- `com.android.vending.BILLING`
- `android.permission.MOUNT_UNMOUNT_FILESYSTEMS`

## Screen support

- normalScreens: `true`
- largeScreens: `true`
- anyDensity: `true`

## Application components

### Activities

- `com.hippiegame.nevergone.TJ_P_01` (launcher)

### Services

- none

### Receivers

- none

### Providers

- none

## Application metadata

- `com.google.android.gms.version`: `@0x7f060000`

## Reproduction

```bash
python3 tools/apk_manifest_summary.py /path/to/com.hippiegame.nevergone.apk \
  --json build/original-manifest.json \
  --markdown build/original-manifest.md
```

The generated Markdown for the known baseline APK should match the metadata above. Resource references are intentionally left as resource IDs rather than resolving and copying proprietary string/resource tables.
