#!/usr/bin/env python3
"""Add explicit Java Module imports for JDK 17 to a local unidbg checkout."""
import argparse
from pathlib import Path

FILES = (
    'unidbg-android/src/main/java/com/github/unidbg/linux/AndroidElfLoader.java',
    'unidbg-api/src/main/java/com/github/unidbg/arm/AbstractARMDebugger.java',
    'unidbg-ios/src/main/java/com/github/unidbg/ios/MachOLoader.java',
)


def prepare(root):
    for name in FILES:
        path = root / name
        text = path.read_text()
        needle = 'import com.github.unidbg.*;\n'
        added = 'import com.github.unidbg.Module;\n'
        if added in text:
            continue
        if text.count(needle) != 1:
            raise ValueError(f'Unexpected source layout: {path}')
        path.write_text(text.replace(needle, needle + added))
        print('Added explicit Module import:', name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('unidbg_checkout', type=Path)
    prepare(parser.parse_args().unidbg_checkout)
