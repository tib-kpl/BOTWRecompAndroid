"""Per-instruction replacements applied by gen.py (guest address -> function(gen, addr) returning C code).

Each entry replaces the one instruction at that address of `main` (canonical address, 0x7000000000 + offset) with the C
the function returns; gen.py rebases the addresses in it for --dyn-base. Check the instruction word first (an assert),
so a wrong build fails here instead of patching the wrong code. Example, from LAS Recomp's widescreen: a camera's
`ldr sN, [xM, #imm]` of the 16:9 aspect literal, replaced by a call into the runtime:

    def _aspect_load(gen, a):
        w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
        assert (w & 0xffc00000) == 0xbd400000, hex(w)  # ldr s, [x, #imm]
        rt = w & 31
        return (f'{{ extern float rt_aspect_site(int); float f = rt_aspect_site(0); '
                f'c->v[{rt}] = 0; memcpy(&c->v[{rt}], &f, 4); }}  /* {a:#x}: ldr s{rt}, =16/9 */')

    PATCHES[0x7000xxxxxx] = _aspect_load

BOTW v1.0.0 (the code addresses are that build's; docs/BOTW-PORT.md):
- 0x70010c1aa0: act::ClusteredRenderer's teardown (f_70010c1a04) calls the virtual destroy of its display-list heap
  at this+0xc60 without a null check. Its setup (f_70010c084c) sets the "created" flag even when that 7 MB heap
  could not be made - here OpenWorldStage runs ~13 MB short and it never is - so a reload (dying, loading a save)
  crashed. A null heap now skips the destroy (the field and the flag are cleared as before).
"""
import struct

PATCHES = {}      # v1.0.0 (main build 2094f152...)
PATCHES_160 = {}  # v1.6.0 (main build 8e9978d5...): the same fixes at that build's addresses
PATCHES_182 = {}  # v1.8.2 (main build de0b6ac4...)
PATCHES_190 = {}  # v1.9.0 (main build cd57b23f...)
BUILD_1_0_0 = '2094f152836683a3632c6153fd84ab80'
BUILD_1_6_0 = '8e9978d50bdd20b4c8395a106c27ffde'
BUILD_1_8_2 = 'de0b6ac4efc1dda7f80f6d7598ec7a7d'
BUILD_1_9_0 = 'cd57b23fa4bbad65803d9788c01821ee'


def _clustered_dl_heap_null(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xf9400008, hex(w)  # ldr x8, [x0]
    return ('if (c->x[0] == 0) { c->x[21] = c->x[20] + 0xc60ull; goto L_70010c1ad8; }  /* (tools/patches.py) */ '
            '{ uint64_t ea = c->x[0] + 0x0ull; uint64_t v = LD64(ea); c->x[8] = (uint64_t)(v); }')


PATCHES[0x70010c1aa0] = _clustered_dl_heap_null


# - 0x7000c4f4d4: sead's frame loop (f_7000c4f430) picks the present interval from the last frame's duration
#   (this+0x240, ticks): floor(vsyncs) + 1, or + 2 past a 0.9 fraction, never under 2. A frame over ~1.9 vsyncs takes it
#   to 20 fps (interval 3), where our GPU's clocks settle low and it stays. The menu's fixed frame rate (Video: Frame
#   rate 30 / 60) caps the duration read here (rt_frame_ticks), so the game itself keeps 2 - its state matches what is
#   presented; a frame that doesn't fit plays slightly slower. Auto: the game's own choice.
def _frame_ticks(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xf9412269, hex(w)  # ldr x9, [x19, #0x240]
    return ('{ extern int64_t rt_frame_ticks(int64_t); uint64_t ea = c->x[19] + 0x240ull; '
            'c->x[9] = (uint64_t)rt_frame_ticks((int64_t)LD64(ea)); }  /* (tools/patches.py) */')


PATCHES[0x7000c4f4d4] = _frame_ticks


# v1.6.0 (found by the same code: the instructions around each 1.0.0 site; its structs moved by a few bytes)
# - 0x7001861440: act::ClusteredRenderer's teardown - here without the "created" check of 1.0.0: it reads the
#   display-list heap at this+0xbf0 and calls its virtual destroy unconditionally; a null heap now goes on to the
#   flags' update (w8 = this+0xc9c, as the destroy's two paths load it) and the field's clearing at 0x7001861470.
def _clustered_dl_heap_null_160(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xf9400008, hex(w)  # ldr x8, [x0]
    return ('if (c->x[0] == 0) { uint64_t ea = c->x[20] + 0xc9cull; c->x[8] = (uint64_t)(uint32_t)LD32(ea); '
            'goto L_7001861470; }  /* (tools/patches.py) */ '
            '{ uint64_t ea = c->x[0] + 0x0ull; uint64_t v = LD64(ea); c->x[8] = (uint64_t)(v); }')


PATCHES_160[0x7001861440] = _clustered_dl_heap_null_160


# - 0x700161495c: sead's frame loop, the last frame's duration at this+0x248 (1.0.0: +0x240, into x9)
def _frame_ticks_160(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xf9412668, hex(w)  # ldr x8, [x19, #0x248]
    return ('{ extern int64_t rt_frame_ticks(int64_t); uint64_t ea = c->x[19] + 0x248ull; '
            'c->x[8] = (uint64_t)rt_frame_ticks((int64_t)LD64(ea)); }  /* (tools/patches.py) */')


PATCHES_160[0x700161495c] = _frame_ticks_160


# v1.8.2, frame rate (runtime/hle/framepace.c; SavageWindRecomp's patches, MIT): the runtime paces the frames at the
# present and sets the game's clock. The frame loop's vsync wait (sead framework, 0x7000d811fc; only the first frames
# after start-up come here) is skipped (the game's own runs with LARECOMP_FRAMEPACE=0). ksys::VFR::updateInterval
# (x19 = VFR) has just set the interval ratio the game's time step comes from: the runtime puts the frame's real length
# there, before the time multipliers and the deltas are worked out from it.
def _frame_wait_182(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xa9bd7bfd, hex(w)  # stp x29, x30, [sp, #-0x30]!
    return ('{ extern int rt_frame_wait(void); if (rt_frame_wait()) return; '
            'uint64_t ea = c->sp - 0x30; ST64(ea, c->x[29]); ST64(ea + 8, c->x[30]); c->sp = ea; }'
            f'  /* {a:#x}: stp x29, x30, [sp, #-0x30]! (frame wait; tools/patches.py) */')


def _vfr_ratio_182(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xb9407a68, hex(w)  # ldr w8, [x19, #0x78]
    return ('{ extern void rt_vfr_update(uint64_t); rt_vfr_update(c->x[19]); c->x[8] = LD32(c->x[19] + 0x78); }'
            f'  /* {a:#x}: ldr w8, [x19, #0x78] (VFR: interval ratio; tools/patches.py) */')


# sead::PerspectiveProjection::doUpdateMatrix (x0 = the projection; the vtable at 0x7001ba4438, entry 11): before it
# reads near, far, tan and aspect, the runtime widens a 16:9 aspect to the output's shape (runtime/hle/widescreen.c
# rt_persp_update: the main camera, whose aspect the game sets back to 16:9 before each rebuild).
def _persp_matrix_182(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xbd409800, hex(w)  # ldr s0, [x0, #0x98]
    return ('{ extern void rt_persp_update(uint64_t); rt_persp_update(c->x[0]); c->v[0] = LD32(c->x[0] + 0x98); }'
            f'  /* {a:#x}: ldr s0, [x0, #0x98] (projection matrix: widescreen; tools/patches.py) */')


# sead::PerspectiveProjection::getAspect (vtable entry 7): the widened aspect too (runtime/hle/widescreen.c
# rt_persp_aspect), so the code asking for it agrees with the matrix.
def _persp_get_aspect_182(gen, a):
    w = struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]
    assert w == 0xbd40b000, hex(w)  # ldr s0, [x0, #0xb0]
    return ('{ extern uint32_t rt_persp_aspect(uint64_t); c->v[0] = rt_persp_aspect(c->x[0]); }'
            f'  /* {a:#x}: ldr s0, [x0, #0xb0] (projection aspect: widescreen; tools/patches.py) */')


PATCHES_182[0x7000d811fc] = _frame_wait_182
PATCHES_182[0x70014092f4] = _vfr_ratio_182
PATCHES_182[0x7000da13f4] = _persp_matrix_182
PATCHES_182[0x7000da13d8] = _persp_get_aspect_182

# v1.9.0: verified against this build's main, not inferred from an address delta.
# waitVsync at 0xe9487c has the same prologue; VFR::updateInterval at 0x151d3d0
# writes +0x30/+0x3c just before 0x151d494 and then updates its time multipliers.
# Both instruction replacements validate the original word before generation.
PATCHES_190[0x7000e9487c] = _frame_wait_182
PATCHES_190[0x700151d494] = _vfr_ratio_182


def patches_for(build_id):
    """The patches for a module, by its build id (sdk and anything unknown: none)."""
    return {BUILD_1_0_0: PATCHES, BUILD_1_6_0: PATCHES_160, BUILD_1_8_2: PATCHES_182,
            BUILD_1_9_0: PATCHES_190}.get(build_id[:32], {})
