# Host startup probe

`tools/run_startup_probe.py` executes the recovered `Game.StartLua` chain outside Android so the next runtime compatibility blocker can be identified without repeatedly installing an APK.

The probe accepts a user's own original Never Gone APK and a Lua 5.2 executable. It decodes only `.lua` entries from `assets/assets/Script/` into a temporary directory, loads the same embedded compatibility script used by the Android runtime, installs boot-safe offline shims for already reconstructed platform/filesystem/UI/ProtoRPC/XML surfaces, and executes `Game.StartLua` through `CAddDoString`.

Example after preparing the pinned Lua 5.2.3 source:

```bash
python3 tools/fetch_lua_5_2_3.py
make -C third_party/_local/lua-5.2.3 generic
python3 tools/run_startup_probe.py /path/to/com.hippiegame.nevergone.apk \
  --lua third_party/_local/lua-5.2.3/src/lua
```

The report includes:

- whether `Game.StartLua` completed;
- the ordered list of modules requested through `CAddDoString`;
- globals that were read before being defined;
- the full Lua traceback on failure.

Missing globals retain ordinary Lua `nil` semantics while being recorded, matching the Android runtime's diagnostic probe. This matters for optional guards such as `if SomeNativeFunction ~= nil then ... end`.

By default all decoded scripts are removed when the probe exits. `--keep-temp` may be used for local debugging; the resulting decoded scripts are proprietary game data and must not be committed.

The probe deliberately keeps network operations offline. It is a boot/reconstruction diagnostic, not a replacement for the eventual networking implementation.