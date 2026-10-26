#!/usr/bin/env python3
"""Exercise native version paths and NSO identification without game files (Linux/WSL)."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='botw-versions-') as temp:
    out = Path(temp)
    (out / 'SDL3').mkdir()
    (out / 'SDL3/SDL.h').write_text('''
#pragma once
#define SDL_MESSAGEBOX_ERROR 0
int SDL_ShowSimpleMessageBox(unsigned, const char *, const char *, void *);
typedef struct SDL_Surface SDL_Surface;
typedef struct SDL_Window SDL_Window;
SDL_Surface *SDL_LoadPNG(const char *);
void SDL_SetWindowIcon(SDL_Window *, SDL_Surface *);
void SDL_DestroySurface(SDL_Surface *);
''')
    loader = (root / 'runtime/loader.c').read_text()
    start = loader.index('int game_version_of_build(')
    identify = loader[start:loader.index('\n}\n', start) + 3]
    (out / 'test.c').write_text('''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "paths.c"
int SDL_ShowSimpleMessageBox(unsigned f, const char *t, const char *m, void *w) { return 0; }
bool plat_exe_path(char *out, size_t n) { return false; }
''' + identify + r'''
int main(int argc, char **argv) {
    const uint8_t ids[][8] = {
        {0x20,0x94,0xf1,0x52,0x83,0x66,0x83,0xa3},
        {0x8e,0x99,0x78,0xd5,0x0b,0xdd,0x20,0xb4},
        {0xde,0x0b,0x6a,0xc4,0xef,0xc1,0xdd,0xa7},
        {0xcd,0x57,0xb2,0x3f,0xa4,0xbb,0xad,0x65}, {0}};
    const int versions[] = {100,160,182,190,0};
    for (int i=0; i<5; i++) assert(game_version_of_build(ids[i]) == versions[i]);
    char main[1024], version[1024], game[1100], file[1100], lib[1100];
    snprintf(main, sizeof main, "%s/data", argv[1]);
    mkdirs(main);
    snprintf(file, sizeof file, "%s/botwrecompandroid.ini", main);
    FILE *f = fopen(file,"wb"); fputs("preserve=base\n",f); fclose(f);
    for (int i=0; i<4; i++) {
        if (i) snprintf(version,sizeof version,"%s/v%d",main,versions[i]);
        else snprintf(version,sizeof version,"%s",main);
        setenv("LARECOMP_DATA_ROOT",version,1);
        unsetenv("LARECOMP_SAVE_SEED");
        rt_paths_init();
        assert(rt_game_version()==versions[i]);
        assert(rt_version_root(100,file,sizeof file) && !strcmp(main,file));
        snprintf(game,sizeof game,"%s/extracted",version); mkdirs(game);
        assert(rt_game_dir_version(game)==versions[i]);
        assert(rt_version_game_lib(versions[i],lib,sizeof lib)==(i!=0));
        if (i) {
            snprintf(file,sizeof file,"%s/extracted/botwgame.so",version);
            assert(!strcmp(lib,file));
            snprintf(file,sizeof file,"%s/save",main);
            assert(!strcmp(getenv("LARECOMP_SAVE_SEED"),file));
            f=fopen("botwrecompandroid.ini","rb"); assert(f);
            char value[64]; assert(fgets(value,sizeof value,f)); fclose(f);
            assert(!strcmp(value,"preserve=base\n"));
        }
        assert(rt_select_game_version(versions[i]));
        assert(selected_version(main)==versions[i]);
    }
    assert(!rt_version_root(999,file,sizeof file));
    assert(!rt_version_game_lib(999,lib,sizeof lib));
    assert(rt_select_game_version(999)); assert(selected_version(main)==100);
    puts("Native game version identification, isolated paths, settings and save seeds OK");
}
''')
    exe = out / 'test'
    subprocess.run(['clang', '-std=gnu11', '-ffunction-sections', '-Wl,--gc-sections',
                    '-I' + str(out), '-I' + str(root / 'runtime'),
                    str(out / 'test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(out)], check=True)
