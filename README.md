# BOTWRecompAndroid

**English** | [Français](README.fr.md) | [Português](README.pt-BR.md)

> [!NOTE]
> **Purpose of this fork**
>
> This fork of [mamcjr/BOTWRecompAndroid](https://github.com/mamcjr/BOTWRecompAndroid) focuses on making Android builds
> easy to download and the application's menus accessible in more languages. Its current additions are:
>
> - **Automatic APK releases:** every successful push to `main` builds a signed Android APK and publishes it as a
>   prerelease on [this fork's Releases page](https://github.com/tib-kpl/BOTWRecompAndroid/releases).
> - **French menus:** French translations for the launcher, settings, controls, Android dialogs and shortcuts,
>   alongside the existing English and Portuguese menus. The interface follows the Android app language.
> - **Support for future translations:** separate translation catalogs, Android string resources and CI checks make
>   it easier to add more languages. See [the translation guide](docs/TRANSLATIONS.md).
>
> The recompilation engine and existing game support come from the upstream projects credited below. The menu
> translations described here concern the application interface; the game's text language has its own setting.
>
> See [Android CI releases](docs/ANDROID-RELEASES.md) for build and signing details.

A static recompilation of *The Legend of Zelda: Breath of the Wild* (Nintendo Switch, versions **1.0.0, 1.6.0 and
1.8.2**) for Android. This is **not emulation**: the game's ARM64 code becomes C compiled for your phone, and its
graphics (NVN) are translated to Vulkan.

It is built on the engine of **[LAS Recomp](https://github.com/peiikai/LASRecomp)** by peiikai (a recompilation of
*Link's Awakening*) and its Android port, **[LASRecompAndroid](https://github.com/mamcjr/LASRecompAndroid)**.

> [!WARNING]
> **Beta.** Tested on one phone only (POCO F7 Ultra, Adreno 830, Mesa Turnip driver). Expect bugs. The plan and the
> state of each phase are in [docs/BOTW-PORT.md](docs/BOTW-PORT.md) (in Portuguese).

## Supported platform

Android is the only application target. Desktop ports and packaging were discontinued on 2026-10-22.
Windows, Linux or macOS may still be used as development hosts with the Android SDK/NDK.

## Status

- **Playable** from the title screen into the world, with saves, on the POCO F7 Ultra: 1.0.0 and 1.6.0 at 30 fps,
  1.8.2 at 30 or up to 60 (about 40-47 fps in the open world with a Turnip driver).
- **1.8.2** has Brazilian Portuguese, widescreen (Video > Aspect ratio "Unlocked": the view widens, the HUD goes to
  the screen's edges) and SavageWindRecomp's frame pacing.
- **Video:** internal resolution, AMD FSR 1 / Snapdragon GSR 1 upscaling, output resolution, SMAA.
- **App:** game files set up from the phone (each version in its own folder, switched on the Game Files page), touch
  controls, controllers, mods, menus in English, French and Portuguese, Vulkan driver choice (Turnip via adrenotools) and
  driver compatibility options.
- **Known gaps:** setting the game up on the phone compiles the game code there, and that has not been timed or
  tested with BOTW yet (expect a long wait and a hot phone); GPUs other than Adreno 8xx take slower, safer paths;
  thumbnails of saves made in the app are black; the audio still uses a simplified mixer.

> [!IMPORTANT]
> Like LAS Recomp, this project is made almost entirely with AI (Claude, by Anthropic, through Claude Code), directed,
> tested and reviewed by a human.

> [!NOTE]
> **No game files are included.** You need your own legally obtained dump of *Breath of the Wild* 1.0.0, 1.6.0 or
> 1.8.2 (`exefs` and `romfs`, extracted). Instructions on how to obtain it will not be provided.

## Controller compatibility

**Built-in controllers — AYN Thor, Odin 2 and Retroid:** standard buttons, both sticks and triggers are expected
to work through SDL3's Android gamepad support, provided the device is compatible with the APK. These handhelds
have **not yet been tested with this fork**. Open **Settings → Controls** to check detection and remap buttons.
The default layout is Nintendo-style: **A on the right, B at the bottom**.

## Installing (beta)

1. Download the APK from [Releases](https://github.com/tib-kpl/BOTWRecompAndroid/releases) and install it (Android 11+,
   arm64, Vulkan; about 30 GB free per game version).
2. To keep game data on an SD card, first open **Settings → Files → Game data storage** and select the card.
   The choice is saved across launches; changing storage restarts the app and does not move existing files.
3. Go to **Set up game files** and choose the folder with your `exefs` and `romfs`. The version is told from
   the files. The app copies what it needs and builds the game code on the phone.
4. On Adreno phones, a Mesa Turnip driver (Settings > Files > Vulkan driver) runs the game much faster than the
   system driver. If the app stops at start with "Vulkan driver is too old", long-press the app icon and choose
   **Choose driver** to pick one before the game starts.

## Testing on Android (developers)

With the Android SDK / NDK, a phone on adb and your dump in `Dump/`:

```bash
cd android/botwrecompandroid && ./gradlew assembleDebug    # the app
android/build_gamelib.sh Dump/01007EF00011E000/exefs  # the game code (about an hour)
android/push_game.sh Dump/01007EF00011E000           # game files and game code onto the phone
android/run_boot.sh 120                               # start the game, log to android/out/boot.log
```

## Credits

- [peiikai](https://github.com/peiikai) for [LAS Recomp](https://github.com/peiikai/LASRecomp), the recompiler and
  runtime this project starts from (MIT, see [LICENSE](LICENSE)). The original README is kept in [docs/las/](docs/las/).
- [LASRecompAndroid](https://github.com/mamcjr/LASRecompAndroid), its Android port: the app, the phone-side game code
  build, touch controls, mods, driver compatibility and the Portuguese menus.
- [Revivedx](https://github.com/Revivedx) for [SavageWindRecomp](https://github.com/Revivedx/SavageWindRecomp) (MIT),
  the reference for 1.8.2: frame pacing, internal resolution, the SDK 20 kernel fixes, setjmp.
- [Nycolazs](https://github.com/Nycolazs) for LASRecomp-Android-Plus (MIT): Vulkan driver compatibility.
- The [zeldaret/botw](https://github.com/zeldaret/botw) decompilation and [open-ead/sead](https://github.com/open-ead/sead),
  for what is known about the game's code.
- Dusklight's launcher, the start screen's style.
- Upscaling and anti-aliasing: [AMD FidelityFX FSR 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR) (MIT),
  [Qualcomm Snapdragon GSR](https://github.com/SnapdragonStudios/snapdragon-gsr) (BSD-3-Clause),
  [SMAA](https://github.com/iryoku/smaa) (MIT).
- Libraries: [SDL](https://libsdl.org) (zlib), [Dear ImGui](https://github.com/ocornut/imgui) (MIT),
  [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD-2-Clause), [volk](https://github.com/zeux/volk) (MIT),
  [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (MIT),
  [shaderc](https://github.com/google/shaderc) (Apache 2.0), [FFmpeg](https://ffmpeg.org) (LGPL),
  [LZ4](https://github.com/lz4/lz4) (BSD-2-Clause), [Capstone](https://www.capstone-engine.org) (BSD-3-Clause),
  [Unicorn](https://www.unicorn-engine.org) (GPLv2, built from its source by `android/deps/build_unicorn.sh`),
  [Python](https://www.python.org) (PSF) and [Zig](https://ziglang.org) (MIT) for the phone-side build.
- [Mesa](https://www.mesa3d.org)'s Turnip driver (MIT), which players load themselves.
- Fonts: [Baloo 2](https://github.com/EkType/Baloo2) (OFL), DejaVu Sans (Bitstream Vera license). The Android robot
  is reproduced or modified from work created and shared by Google (CC BY 3.0).
- Made with [Claude Code](https://claude.com/claude-code) (Anthropic).

*The Legend of Zelda* and *Breath of the Wild* are trademarks of Nintendo. This project is not affiliated with or
endorsed by Nintendo.
