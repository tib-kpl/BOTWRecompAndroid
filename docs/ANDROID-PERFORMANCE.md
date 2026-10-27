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
