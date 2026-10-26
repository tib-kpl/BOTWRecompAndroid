#!/usr/bin/env python3
"""Compile the exact built-in temporal shader, without requiring a running game."""
import ast
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
header = (root / 'runtime/nvn/vulkan/temporal_shader.h').read_text(encoding='utf-8')
body = header.split('static const char temporal_shader_src[] =', 1)[1]
source = ''.join(ast.literal_eval(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', body))
out = root / 'build/temporal-tests'
out.mkdir(parents=True, exist_ok=True)
shader = out / 'temporal.frag'
shader.write_text(source, encoding='utf-8')
subprocess.run([sys.argv[1], '--target-env=vulkan1.1', str(shader), '-o', str(out / 'temporal.spv')], check=True)
print('Production temporal shader compiled successfully')
