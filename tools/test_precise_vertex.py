#!/usr/bin/env python3
"""Check precision before optimization across register bitcasts and helper returns."""
import argparse
import struct
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cc', default='cc')
p.add_argument('--glslc', default='glslc')
p.add_argument('--generate-only', action='store_true')
p.add_argument('--generated-only', action='store_true')
args = p.parse_args()
out = root / 'build/precise-vertex-tests'
out.mkdir(parents=True, exist_ok=True)
helper = out / 'check'
if not args.generated_only:
    subprocess.run([args.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    str(root / 'tools/test_precise_vertex.c'), '-o', str(helper)], check=True)
    subprocess.run([str(helper)], check=True)
    base = '''#version 460
layout(location=0) in vec3 a;
layout(location=1) in vec3 b;
layout(location=0) out vec4 colour;
float f(uint x) { return uintBitsToFloat(x); }
uint u(float x) { return floatBitsToUint(x); }
float helper(float x, float y, float z) { return x*y+z; }
void main() {
  uint r0 = u(a.x);
  uint r1 = u(b.x);
  uint r2 = u(a.y);
  uint r3 = u(EXPRESSION);
  bool predicate = a.x*b.x+a.y > 0.0;
  colour = vec4(f(r3));
  gl_Position = vec4(f(r3), predicate ? a.z : b.z, 0, 1);
}
'''
    for name, expr in [('registers', 'f(r0)*f(r1)+f(r2)'),
                       ('helper', 'helper(f(r0),f(r1),f(r2))'),
                       ('explicit-fma', 'fma(f(r0),f(r1),f(r2))')]:
        source = out / (name + '.vert')
        source.write_text(base.replace('EXPRESSION', expr))
        qualified = out / (name + '-precise.vert')
        qualified.write_bytes(subprocess.check_output([str(helper), 'source', str(source)]))
if args.generate_only:
    raise SystemExit(0)

def instructions(path):
    binary = path.read_bytes()
    words = struct.unpack('<' + 'I' * (len(binary) // 4), binary)
    i = 5
    result = []
    while i < len(words):
        n = words[i] >> 16
        assert n and i + n <= len(words)
        result.append((words[i] & 65535, words[i:i+n]))
        i += n
    return result

for name in ['registers', 'helper', 'explicit-fma']:
    for optimized in [False, True]:
        spv = out / (name + ('-opt' if optimized else '-raw') + '.spv')
        subprocess.run([args.glslc, '--target-env=vulkan1.3', '-O' if optimized else '-O0',
                        str(out / (name + '-precise.vert')), '-o', str(spv)], check=True)
        ops = instructions(spv)
        marked = {w[1] for op, w in ops if op == 71 and w[2] == 42}
        arithmetic = [w for op, w in ops if op in [129, 131, 133, 136, 142, 143, 144, 145, 146, 148]]
        assert arithmetic and all(w[2] in marked for w in arithmetic), (name, optimized)
        fma = sum(op == 12 and w[4] == 50 for op, w in ops)
        assert fma == (1 if name == 'explicit-fma' else 0), (name, optimized, fma)
        if not args.generated_only:
            subprocess.run([str(helper), 'check', str(spv)], check=True)
print('Register/helper precision survives optimization; explicit Fma preserved (6 shader compilations)')
