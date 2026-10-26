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
