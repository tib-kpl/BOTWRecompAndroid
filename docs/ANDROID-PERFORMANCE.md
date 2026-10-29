# Android performance measurements

[Français](ANDROID-PERFORMANCE.fr.md)

## Odin 2 Portal: driver selection matters

Measured on 26 October 2026 with an Odin 2 Portal (Snapdragon 8 Gen 2, Adreno 740, 8 GB RAM, Android 13),
BOTW 1.0.0 and the same save in the Shrine of Resurrection corridor. Link and the camera stayed still.
Measurements below exclude loading and initial shader compilation.

The controlled comparison used 50% internal resolution, SGSR 1, no SMAA, screen output and the 30 fps setting.

| AYN power mode | App Vulkan driver | Observed steady frame rate |
| --- | --- | --- |
| Standard | MrPurple T30, Mesa 26.3.0, git `62ac221a33` | about 14.9 fps |
| Performance | Same T30 driver | about 18 fps |
| Performance | Banner Turnip `v27.0.0-20261025-r5`, generic A6xx/A7xx package, Mesa git `ab1502f` | 29.9–30.1 fps |

These are scene-specific results, not a guarantee for the entire game or other handhelds.

Restoring the player's usual dynamic resolution, FSR 1 and SMAA settings gave about 29–30 fps with the corrected
driver in Performance mode in the same corridor. Those settings are separate from the controlled table above.

### Tested driver

The working package is `Turnip-v27.0.0-20261025-r5.zip` from
[The412Banner/Banners-Turnip](https://github.com/The412Banner/Banners-Turnip/releases/tag/v27.0.0-20261025-r5).
It is the generic A6xx/A7xx Android package; the A8xx, OneUI, Linux and Wayland variants were not used.
Import the ZIP with the app's driver picker and restart the app. Shader caches may need to warm up after changing drivers.
Driver compatibility settings remained Automatic; placing the entire game heap in GPU memory was not required.

### Why the old driver was slow

The CPU profile spent substantial wall time in Turnip's `wait_timestamp_safe`. Inspection of the tested T30 binary
confirmed that an expired/zero timeout goes directly to the KGSL wait ioctl. That kernel interface treats zero as
an indefinite wait, turning timeline polling into synchronization with unfinished GPU work.

Mesa MR 44838 fixes this by polling the retired timestamp instead. The driver builder documents the upstream fix
and checks for it in its build scripts:
[KGSL fix provenance](https://github.com/The412Banner/Banners-Turnip/blob/A8xx/patches/common/SOURCE).
Updating the driver removed the observed serialization; the same scene then reached its 30 fps cap.

## 30 fps and 60 fps are separate tests

The tested game is 1.0.0. Selecting 60 fps still left this scene at 30 fps. The version-specific frame pacing and
game-clock adaptation in `runtime/hle/framepace.c` currently apply to 1.8.2. A 30 fps result on 1.0.0 does not establish
the device's maximum unlocked rendering rate, and a correct 60 fps gameplay result has not been demonstrated here.

## Profiling and rejected experiments

Detailed per-draw profiling changes the workload. Use light frame-time logs for comparisons, after shader compilation
has settled, and keep the scene, settings, power mode and driver identical.

`LARECOMP_TIMING_TRACE=1` reports explicit timeline waits, swapchain acquisition/presentation, submissions and timeline
polls separately. A long nominally nonblocking poll is useful evidence of a driver problem.

On T30, alternating tests of descriptor binding plans and a four-way uniform-copy cache produced no useful gain.
An incremental GPU snapshot prototype reduced CPU uniform copies by about 92%, but was about 2% slower overall.
The unsuccessful cache/snapshot variants were removed. The copy-cache invalidation fix and its regression tests remain.

## BOTW 1.9.0: NVN command pages, 26 October 2026

Odin 2 Portal, Turnip `v27.0.0-20261025-r5`, 60 fps target, dynamic resolution, FSR1 and SMAA.
Android `performance_mode=2` was recorded before and after each comparison; the owner identifies this as High
Performance. These measurements must not be called a Standard-mode baseline.

CPU sampling attributed about 4.7% of the samples to command-page lookup and its locking. Pages are never freed,
so publishing their keys atomically lets readers access existing pages without locking. Allocation remains locked.
Tests cover collisions and 480,000 concurrent lookups by eight threads; the fast-path test also passes ThreadSanitizer.

One-minute alternating test in the shrine, without detailed per-draw profiling:

| Path | Measured frames | Throughput from frame periods | Process CPU counters per second |
| --- | ---: | ---: | ---: |
| Locked lookup | 1,200 | 38.37 fps | 220.43 ticks/s |
| Atomic lookup | 1,133 | 38.99 fps | 216.59 ticks/s |

The improvement is small in this test: about 1.6% higher throughput and 3.3% fewer CPU ticks per frame. All four
complete alternating pairs favor the atomic path. This does not establish the same gain throughout the game or
sustained 60 fps.

Driver-constant cache experiments showed no measurable gain and were removed. The current long-pass split remains
enabled (about 38.8 fps versus 37.1 without splitting in that test). Shortening submission intervals from 2 ms to
0.5 ms did not improve throughput. None of these experiments lowers rendering quality.

## BOTW 1.9.0: duplicate GPU writebacks, 27 October 2026

Odin 2 Portal, the same Turnip driver, High Performance (`performance_mode=2`), the same outdoor save and camera,
60 fps target, SMAA and temporal stabilization off. Both internal rendering and screen output were fixed at
**1920×1080** for this comparison; dynamic resolution was bypassed. GPU heap access remained Automatic (CPU-copy
path on this Adreno 740). This is a different workload from the shrine test above.

Several commands registered identical resident memory ranges for writeback from the same GPU submission.
These requests all observe the same completed buffer contents. Coalescing requests with identical submission,
source, destination, extent, shadow and ownership metadata avoids rescanning them. Requests from different
submissions or allocations stay separate, as do unshadowed copies. Newer CPU writes retain their protection.

The test alternated the old and new paths every 120 presented frames in the same process. Complete phases after
warmup, without detailed per-draw profiling:

| Path | Measured frames | Throughput from frame periods |
| --- | ---: | ---: |
| Original writeback queue | 480 | 23.74 fps |
| Coalesced resident writebacks | 600 | 27.65 fps |

This is about **16.5% higher throughput in this scene**. All four complete old/new pairs favor coalescing.
Steady 120-frame windows scanned roughly 8,800–9,100 MiB before coalescing versus about 1,600 MiB after it.
The optimization changes scheduling of redundant CPU work,
not image resolution. Queue identity tests and ownership-merge tests pass, including AddressSanitizer and UBSan.

This does **not** demonstrate sustained 60 fps or fix the known terrain/black-line artifacts. GPU work remains
around 30 ms per frame in this scene. Fragment-only shader optimization and LRZ tests have not established a
separate reliable gain; they are not enabled by this optimization. Direct GPU heap access is still being evaluated.

## Measurements after the upstream 0.2.1 merge

Release 49/51, rebuilt BOTW 1.9.0 game module, the same Odin 2 Portal outdoor test save, High Performance,
corrected Turnip driver, fixed **1920×1080 internal rendering and output**. Tests use a separate save directory.

Upstream's synchronization-lag option improves throughput in this scene. A warmed same-process test alternating
every 120 frames, retaining complete phases only, measured 26.28 fps with lag off (840 frames) and 30.81 fps with
lag on (840 frames). This is an upstream option, not an additional optimization from this fork. It can affect
GPU writeback timing; the test does not establish that character/terrain rendering issues are fixed.

Removing inter-pass barriers in a diagnostic A/B measured 26.24 versus 26.50 fps. That approximately 1% difference
does not justify removing required synchronization. Normal barriers remain enabled.

`LARECOMP_VK_OPT_PRECISE=1` is a **developer experiment, off by default**. It adds GLSL precision qualifiers to
vertex registers, outputs and helper return values before requesting shaderc performance optimization. Both the
unoptimized and optimized modules must retain NoContraction on rounding arithmetic; otherwise compilation falls
back to the original source and policy. Explicit Fma and negation are exempt from this check. The experiment has
its own shader-cache namespace. These checks do not replace visual validation of different depth/colour programs.

The synthetic tests cover register bitcasts, helper returns, predicates, explicit Fma, and malformed SPIR-V.
Local checks of 233 captured vertex programs accepted 231 with the legacy SDK compiler; one used an unsupported
shadow-LOD overload and one failed its optimizer. This compiler result does not establish coverage on every device.
The initial steady 600-frame windows measured 25.68 fps normally and 25.70 fps with the experiment; a subsequent
normal reference measured 26.63 fps. These separate-process runs establish **no reliable performance gain**.
The experiment remains disabled in ordinary launches.

A separate per-pass profile recorded 218 intervals totalling approximately 34.07 ms: 29.44 ms in render passes,
2.88 ms in feedback copies, 0.74 ms in compute dispatches, 0.81 ms in presentation and 0.20 ms between submissions.
Timestamp instrumentation affects this frame; it is not a steady-state FPS measurement. Pass labels report the
guest's requested sizes (often 1600×900), while the fixed rendering scale produces 1920×1080 scene output.
The GPU still needs roughly 31–33 ms in ordinary frames in this scene, compared with a 16.67 ms budget for 60 fps.
There is no demonstrated sustained 60 fps result.

At fixed 1080p, another same-process test compared the existing 3 ms long-pass split with no splitting.
Four complete 120-frame phases per mode measured 26.10 fps with splitting and 23.39 fps without it; all four
neighbouring pairs favour splitting. The existing default is retained. This confirms the scheduling benefit at
this resolution; it is not a new optimization or an additive gain over the other measurements above.

## Adreno 740 automatic compatibility profile, 28 October 2026

Odin 2 Portal, High Performance, corrected Turnip Mesa 27.0.0-devel (`ab1502fd16`), BOTW 1.9.0,
fixed 1920×1080 internal rendering and output. Sync lag and the player's temporal stabilization were on;
antialiasing was off. The separate test-save directory and isolated configuration files left the player's
settings and main save untouched. After shader warmup, each run recorded a 45-second stationary interval.

| Configuration | Recorded frames | Throughput from all recorded frame periods |
| --- | ---: | ---: |
| Guest-memory mapping, target reads and LRZ all off | 1,384 | 30.21 fps |
| Guest-memory mapping only | 1,305 | 28.23 fps |
| Target reads in place only | 1,543 | 33.65 fps |

These are separate-process observations, not an alternating A/B proof. The target-read run was about 11.4% faster
than this reference; it does not establish the same gain elsewhere. Landscape, nearby ground/character and
inventory captures were inspected without an observed new rendering defect in these checks. No sustained
60 fps result is claimed. Earlier runs with camera/application changes were discarded.

Automatic now enables **target reads in place only** for Adreno 740 under Turnip. Guest-memory mapping stays off:
it worked in the tested views but was slower. The LRZ run was interrupted by movement and leaving the app, and
the console was subsequently disconnected; it is not a valid comparison or a confirmed driver failure. LRZ
therefore stays off on this GPU. Enabling all three together has not been validated.

The profile is independent per option. Existing Adreno 8xx defaults and explicit On/Off overrides remain unchanged;
no other Adreno 7xx model is added. Compatibility controls show each option's own automatic choice. Other drivers,
game versions and scenes have not been validated by this test; explicit Off still selects target copies if needed.
