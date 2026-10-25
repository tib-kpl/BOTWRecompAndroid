#!/usr/bin/env python3
"""Reject APKs missing the dependencies needed to compile the player's game on-device."""
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as apk:
    required = {
        'lib/arm64-v8a/' + name for name in (
            'libmain.so', 'libSDL3.so', 'libshaderc_shared.so', 'libavcodec.so',
            'libavformat.so', 'libavutil.so', 'libswscale.so', 'libzig.so',
            'libpymain.so', 'libpython3.14.so', 'libcapstone.so', 'libunicorn.so', 'liblz4.so',
        )
    } | {'assets/gen.id', 'assets/gen/tools/gen.py', 'assets/res/DejaVuSans.ttf'}
    missing = required - set(apk.namelist())
    if missing:
        sys.exit('Incomplete APK: ' + ', '.join(sorted(missing)))
    print('Release APK contains the native runtime and on-device game compiler.')
