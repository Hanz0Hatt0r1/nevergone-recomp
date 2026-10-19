# Manual reconstruction: character-name role submission

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

`CharacterNameLayer::menuCloseCallback(CCObject*)` (`ELF 0x002dd05c`,
RevEngInstr `0x003dd05c`, 1248 bytes) handles the character-name form's submit
button when the callback object's tag is `1`. It reads the text from child tag
`102` as a `CCEditBox`. Empty text takes the error/dialog path and does not
reach role creation.

For non-empty text, the method calls
`ManagementLayer::LGG_CheckStringLegal(CCString)`. A zero result also goes to
the error path. It then checks `strlen(text) <= 18`; this is a byte-length
limit, not a Unicode-character count. Longer input is rejected.

The accepted text is converted to `std::string` and passed to
`LUA_LOGIN::CreateTheRole(std::string const&, int)`. The integer argument is
loaded from `CharacterNameLayer +0x104`; `CharacterNameLayer::CretaUI(int)`
stores its argument at that offset. The selected role/name mapping beyond that
stored integer is not inferred here.

This native callback requires a non-empty, legal, bounded name. It does not
replace empty input with the localized name of the selected character. The
reconstruction's automatic selected-name behavior is therefore a project-level
choice rather than behavior recovered from this callback.

Addresses and control flow above come from manual inspection of the local
original ARMv7 disassembly for the binary identified by the SHA-256 at the top;
the RevEngInstr address uses the repository's `+0x10000` convention. These are
manual disassembly labels, not REA output.
