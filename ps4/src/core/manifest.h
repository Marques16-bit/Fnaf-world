// Manifesto dos assets extraidos (assets/manifest.txt), gerado por tools/pack_assets.py.
//
//   fwmanifest 1
//   window 1280 720
//   placeholder 0|1
//   image <indice> <arquivo.fwi> <largura> <altura> <nome_original>
//   title <indice>
//   frame title frames/title.frame
#pragma once
#include "types.h"

#define MAN_MAX_IMAGES 128

struct ImageEntry {
    char file[64];
    int w;
    int h;
    char orig[96];
};

struct Manifest {
    int win_w;
    int win_h;
    bool placeholder;
    int nimg;
    ImageEntry img[MAN_MAX_IMAGES];
    int title;
    char frame_file[64];
};

bool manifest_parse(const char* text, Manifest* out);
