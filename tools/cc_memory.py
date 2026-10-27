"""The compiler's peak memory for each generated file, with the flags the phone builds the game code with
(runtime/gamebuild.c GAMELIB_CFLAGS; a file with a function over 15,000 labels at -O0, as mark_cold), on the PC's
NDK clang (Windows: the driver runs cc1 in its own process, so its peak working set is the compile's).

  tools/venv/Scripts/python tools/cc_memory.py <generated dir> [-j 4] [--files a.c b.c ...] [--top N] [--all]

Prints one line per file: peak MB, seconds, -O1 / -O0, size; the largest peaks last.
"""
import argparse
import concurrent.futures
import ctypes
import ctypes.wintypes as wt
import os
import re
import subprocess
import tempfile
import time

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK = os.environ.get('ANDROID_HOME') or os.path.join(os.environ.get('LOCALAPPDATA', ''), 'Android', 'Sdk')
CLANG = os.path.join(SDK, 'ndk', '30.0.16248370', 'toolchains', 'llvm', 'prebuilt', 'windows-x86_64', 'bin', 'clang.exe')
FLAGS = ['--target=aarch64-linux-android30', '-O1', '-fno-strict-aliasing', '-Wno-return-type', '-Wno-unused-label',
         '-fvisibility=hidden', '-fPIC', '-mllvm', '-dag-maps-huge-region=500', '-I' + os.path.join(R, 'runtime'),
         '-DGAMELIB_ID="x"']
HEAVY_LABELS = 15000


class PMC(ctypes.Structure):
    _fields_ = [('cb', wt.DWORD), ('PageFaultCount', wt.DWORD), ('PeakWorkingSetSize', ctypes.c_size_t),
                ('WorkingSetSize', ctypes.c_size_t), ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                ('QuotaPagedPoolUsage', ctypes.c_size_t), ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                ('QuotaNonPagedPoolUsage', ctypes.c_size_t), ('PagefileUsage', ctypes.c_size_t),
                ('PeakPagefileUsage', ctypes.c_size_t)]


def heavy(path):
    n = 0
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            if re.match(r'^[a-zA-Z].* f_[0-9a-f]+\(.*\) \{$', line):
                n = 0
            elif line.startswith('L_'):
                n += 1
                if n > HEAVY_LABELS:
                    return True
    return False


def measure(path):
    o0 = heavy(path)
    out = os.path.join(tempfile.gettempdir(), 'ccmem_%d.o' % os.getpid())
    t0 = time.time()
    p = subprocess.Popen([CLANG] + FLAGS + (['-O0'] if o0 else []) + ['-c', path, '-o', out + os.path.basename(path)],
                         stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    _, err = p.communicate()
    pmc = PMC()
    pmc.cb = ctypes.sizeof(PMC)
    ctypes.windll.psapi.GetProcessMemoryInfo(int(p._handle), ctypes.byref(pmc), pmc.cb)
    try:
        os.remove(out + os.path.basename(path))
    except OSError:
        pass
    return (path, pmc.PeakPagefileUsage / 2**20, pmc.PeakWorkingSetSize / 2**20, time.time() - t0, o0,
            os.path.getsize(path), p.returncode, err.decode(errors='replace')[-300:])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('gen')
    ap.add_argument('-j', type=int, default=4)
    ap.add_argument('--files', nargs='*')
    ap.add_argument('--top', type=int, default=0, help='the N largest files too')
    ap.add_argument('--all', action='store_true')
    a = ap.parse_args()
    files = []
    allc = [os.path.join(a.gen, m, f) for m in ('main', 'sdk') for f in os.listdir(os.path.join(a.gen, m))
            if f.endswith('.c')]
    if a.all:
        files = allc
    if a.top:
        files += sorted(allc, key=os.path.getsize, reverse=True)[:a.top]
    for f in a.files or []:
        files.append(f if os.path.isabs(f) else os.path.join(a.gen, f))
    files = list(dict.fromkeys(files))
    res = []
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        for r in ex.map(measure, files):
            path, commit, ws, secs, o0, size, rc, err = r
            print(f'{os.path.relpath(path, a.gen):22} commit {commit:7.0f} MB  ws {ws:7.0f} MB  {secs:6.1f} s  '
                  f'{"-O0" if o0 else "-O1"}  {size / 1e6:5.1f} MB C{"  FAILED " + err if rc else ""}', flush=True)
            res.append(r)
    print('largest:')
    for r in sorted(res, key=lambda r: r[1])[-10:]:
        print(f'  {os.path.relpath(r[0], a.gen):22} {r[1]:7.0f} MB  {r[3]:6.1f} s')


if __name__ == '__main__':
    main()
