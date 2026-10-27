// Where BOTWRecompAndroid keeps its files (runtime/paths.c).
//
// Data root: everything the app writes or the user brings - extracted/ (game files), save/,
// cache/ (shaders / pipelines), fonts/ (imported), larecomp.ini. rt_paths_init() picks it and makes it the
// working directory, so the rest of the runtime opens those by their relative names. In order:
//   1. LARECOMP_DATA_ROOT (tests, scratch installs)
//   2. dev / portable mode: the starting directory already has extracted/ (a source checkout: `./build/botwrecompandroid
//      extracted`, the test scripts) - used as is
//   3. macOS: ~/Library/Application Support/com.mamcjr.botwrecompandroid (the .app bundle itself is never written to)
//      Windows: the folder of the .exe (portable, no installer); refuses to start, with a message, if not writable
//      Linux: $XDG_DATA_HOME/botwrecompandroid (~/.local/share/botwrecompandroid)
//      Android: the app's external files folder (Android/data/<package>/files)
//
// Game versions: that root is v1.0.0's. v1.6.0, v1.8.2 and v1.9.0 each run in a folder of their own inside it, v160 / v182 / v190
// (game files, saves, settings - android/run_ver.sh's test runs use the same ones): the one the start screen chose
// (game_version.txt in the root) becomes the data root instead. Its settings start as a copy of 1.0.0's, its saves too
// (services/fs.c, LARECOMP_SAVE_SEED).
//
// Resources: files that ship with the app (helper programs, the window icon) - next to the executable (the
// Windows folder, the .app's Contents/MacOS), in the .app's Contents/Resources, or in the source tree.
#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RT_BUNDLE_ID "com.mamcjr.botwrecompandroid"

void rt_paths_init(void);         // first thing in main(), after plat_init
const char *rt_data_root(void);   // absolute
bool rt_dev_mode(void);           // dev / portable mode (rule 2 above): e.g. a source checkout

// A shipped file: `name` next to the executable or in Contents/Resources, else `dev_rel` in the source tree (the
// starting directory or the executable's parent; NULL = shipped builds only). Absolute path in out; false = none.
bool rt_resource(const char *name, const char *dev_rel, char *out, size_t n);

// Game versions (100, 160, 182, 190: rt.h game_version_of_build)
int rt_game_version(void);                                // the one whose folder is the data root
bool rt_select_game_version(int v);                       // the one the next start runs (the app restarts into it)
bool rt_version_root(int v, char *out, size_t n);         // its folder (absolute)
int rt_game_dir_version(const char *game_dir);            // whose extracted/ folder game_dir is; 0: none
// Where its game code library is loaded from: 1.6.0's / 1.8.2's / 1.9.0's own (Android: <internal files>/v160|v182|v190, as external
// storage is mounted noexec; elsewhere their extracted/). false for 1.0.0, whose library is looked for as before
// (runtime/gamelib.c: next to the executable, then extracted/).
bool rt_version_game_lib(int v, char *out, size_t n);

// The only way the app deletes game files. Removes the named entry of game_dir (an extracted/ folder) only if
// game_dir is <data root>/extracted itself or a game version's (symlinks resolved; a symlinked extracted/ is refused)
// and the entry is one the setup creates (exefs, romfs, romfs.bin, leftover folders) and not a link. The folder the
// user picked never gets here. true = removed or not there.
bool rt_remove_game_file(const char *game_dir, const char *name);
// All the game files set up there (rt_remove_game_file on each, then the empty folder). Saves and settings stay.
bool rt_remove_game_files(const char *game_dir);
// The same before a new setup there, except the game code's compiled objects (.gamebuild): runtime/gamebuild.c keeps
// them only when they were compiled from the same game code by the same builder, so the folder picked again after a
// build the app didn't finish goes on from there (issue #25: it started over)
bool rt_remove_game_files_for_setup(const char *game_dir);
// Bytes taken by the extracted game files in game_dir (links not followed).
unsigned long long rt_game_files_size(const char *game_dir);

typedef struct SDL_Window SDL_Window;
void rt_window_icon(SDL_Window *w);  // the shipped icon.png, if any

#ifdef __cplusplus
}
#endif
