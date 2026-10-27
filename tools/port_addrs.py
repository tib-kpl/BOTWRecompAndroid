"""Find addresses of one build of `main` in another (a game update): code by the instructions around it, data by the
code that reaches it.

  tools/venv/Scripts/python tools/port_addrs.py <old main> <new main> code:0x7000d811fc data:0x7001cb48a4 ...

Code: the old address's neighbourhood (up to 24 instructions each side) is looked for in the new text with the parts
that move between builds masked - adrp pages, b / bl targets, literal loads, and (on a second try) the low 12 bits that
adrp's add / ldr / str take. The widest window with one match wins. Data: every adrp + add / ldr / str in the old text
that lands on the address is a reference; each reference's code is ported as above and the new adrp pair decoded
there; the most common answer wins. Addresses are canonical (0x7000000000 + offset).
"""
import collections
import struct
import sys

sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
from nso import Nso

BASE = 0x7000000000


def words(nso):
    off, size = nso.text
    return off, list(struct.unpack_from('<%dI' % (size // 4), nso.image, off))


def is_adrp(w): return (w & 0x9f000000) == 0x90000000
def is_add_imm(w): return (w & 0x7f800000) == 0x11000000
def is_ldst_uimm(w): return (w & 0x3b000000) == 0x39000000


def norm(w, loose):
    if is_adrp(w): return w & 0x9f00001f
    if (w & 0x7c000000) == 0x14000000: return w & 0xfc000000  # b / bl
    if (w & 0x3b000000) == 0x18000000: return w & 0xff00001f  # ldr literal
    if loose and is_add_imm(w): return w & 0xffc003ff
    if loose and is_ldst_uimm(w): return w & 0xffc003ff
    return w


def packed(ws, loose):
    return struct.pack('<%dI' % len(ws), *(norm(w, loose) for w in ws))


class Text:
    def __init__(self, path):
        self.nso = Nso(path)
        self.off, self.w = words(self.nso)
        self.packed = {False: packed(self.w, False), True: packed(self.w, True)}

    def index(self, addr): return (addr - BASE - self.off) // 4

    def addr(self, i): return BASE + self.off + i * 4


def find_all(hay, needle):
    out, i = [], hay.find(needle)
    while i >= 0:
        if i % 4 == 0: out.append(i // 4)
        if len(out) > 1: break
        i = hay.find(needle, i + 1)
    return out


def port_code(old, new, addr):
    i = old.index(addr)
    for loose in (False, True):
        for before, after in ((8, 8), (16, 16), (24, 24), (4, 12), (12, 4), (2, 6), (0, 24), (24, 0)):
            lo, hi = max(0, i - before), min(len(old.w), i + after + 1)
            hits = find_all(new.packed[loose], packed(old.w[lo:hi], loose))
            if len(hits) == 1:
                return new.addr(hits[0] + (i - lo)), f'{"loose" if loose else "strict"} -{before}/+{after}'
    return None, 'no unique match'


def adrp_target(text, i):
    w = text.w[i]
    imm = ((w >> 29) & 3) | (((w >> 5) & 0x7ffff) << 2)
    if imm & (1 << 20): imm -= 1 << 21
    return ((text.addr(i) & ~0xfff) + (imm << 12)), w & 31


def pair_target(text, i):
    """adrp at i and the next instruction that uses its register: the address it reaches, or None."""
    page, rd = adrp_target(text, i)
    for j in range(i + 1, min(i + 6, len(text.w))):
        w = text.w[j]
        if is_add_imm(w) and (w >> 5) & 31 == rd and not (w >> 22) & 1:
            return page + ((w >> 10) & 0xfff), j
        if is_ldst_uimm(w) and (w >> 5) & 31 == rd:
            size = w >> 30
            if (w >> 26) & 1 and ((w >> 23) & 1): size = 4  # (q register)
            return page + (((w >> 10) & 0xfff) << size), j
    return None, None


def port_data(old, new, addr, near=0x10):
    refs = []
    for i, w in enumerate(old.w):
        if is_adrp(w):
            t, j = pair_target(old, i)
            if t is not None and addr - near <= t <= addr + near: refs.append((i, t))
    votes = collections.Counter()
    for i, t in refs[:40]:
        a, how = port_code(old, new, old.addr(i))
        if a is None: continue
        ni = new.index(a)
        if not is_adrp(new.w[ni]): continue
        nt, _ = pair_target(new, ni)
        if nt is not None: votes[nt + (addr - t)] += 1
    if not votes: return None, f'{len(refs)} references, none ported'
    best, n = votes.most_common(1)[0]
    return best, f'{n} of {sum(votes.values())} ported references ({len(refs)} found)'


def main():
    old, new = Text(sys.argv[1]), Text(sys.argv[2])
    for arg in sys.argv[3:]:
        kind, a = arg.split(':')
        a = int(a, 16)
        r, how = (port_code if kind == 'code' else port_data)(old, new, a)
        extra = ''
        if r is not None and kind == 'code':
            extra = f' {old.w[old.index(a)]:08x} -> {new.w[new.index(r)]:08x}'
        print(f'{kind:4} {a:#x} -> {r:#x} ({how}){extra}' if r is not None else f'{kind:4} {a:#x} -> ? ({how})')


if __name__ == '__main__':
    main()
