// "Frame" = uma tela do jogo: camadas de imagem + areas clicaveis (hotspots) com botao do controle.
//
//   frame title 1280 720
//   layer title 0 0          (imagem "title" = candidato selecionado; ou um indice numerico)
//   layer 5 100 200          (imagem 5 na posicao x=100 y=200, em pixels do jogo)
//   hotspot Jogar 440 360 400 70 cross
#pragma once
#include "padicons.h"

#define FRAME_MAX_LAYERS 64
#define FRAME_MAX_HOTSPOTS 32
#define LAYER_IMAGE_TITLE (-1)

struct Layer {
    int image;  // indice no manifesto ou LAYER_IMAGE_TITLE
    int x;
    int y;
};

struct Hotspot {
    char label[40];
    int x;
    int y;
    int w;
    int h;
    PadBtn btn;
};

struct Frame {
    char name[32];
    int w;
    int h;
    int nlayers;
    Layer layers[FRAME_MAX_LAYERS];
    int nhs;
    Hotspot hs[FRAME_MAX_HOTSPOTS];
};

bool frame_parse(const char* text, Frame* out);
