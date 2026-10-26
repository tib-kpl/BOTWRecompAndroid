# Experimental BOTW 1.9.0 support

The fork recognizes the Switch 1.9.0 main build `cd57b23fa4bbad65`. Import matching `exefs` and `romfs` folders
through the app's game files page. Executables and game data must both come from the updated game; a patch NSP
alone is not an extracted game folder.

The installation uses `<selected storage>/v190/extracted`, with its own settings, saves and caches. On Android,
selecting the SD card keeps those files on the SD card. The executable game library is copied into the app's
private internal storage because Android cannot load executable libraries from the SD card.

Settings and saves start from copies of the 1.0.0 installation. The original saves remain separate; subsequent
progress in 1.9.0 does not update the 1.0.0 saves. Choose another installed version in Settings → Game files →
Versions and setup to switch back.

## Verified on Odin 2 Portal

Tested on 26 October 2026 with the 1.9.0 game files, Turnip `v27.0.0-20261025-r5`, Standard performance mode,
dynamic internal resolution, FSR1 and SMAA:

- Title screen reports **Ver. 1.9.0** and displays French game text.
- An existing 1.0.0 save loads in the Shrine of Resurrection; Link renders and responds to input.
- The French runtime settings menu opens and returns to the game.
- Once initial shaders are compiled, observed samples in the tested room are around **30 fps**. This is not a
  whole-game performance or compatibility guarantee. First-time shader compilation causes visible pauses.

The 60 fps and widescreen hooks remain exclusive to 1.8.2. Their hard-coded addresses must not be reused for
1.9.0. The 1.9.0 update is experimental; other areas and game events still need testing.

## Runtime differences

- Exact build recognition, version selection and separate `v190` paths.
- ROMFS validation checks `System/Version.txt` against 1.9.0 and a baseline of 41,062 files.
- NVN 55.15 device information, as requested by this build.
- SDK timed condition waits return without reacquiring the mutex on timeout. In the 1.9.0 SDK,
  `InternalConditionVariableImplByHorizon::TimedWait` at offset `0x149354` explicitly enters the mutex again after
  result `0xea01`; reacquiring it twice deadlocks.
- Android's chosen storage volume is applied before the saved version selection. An explicit
  `LARECOMP_DATA_ROOT` test override still takes precedence.

Native version/path tests and ROMFS validation run in the release CI. No game executable, generated game code,
game data or console keys are distributed with the APK or this repository.
