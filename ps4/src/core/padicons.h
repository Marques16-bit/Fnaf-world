// Icones dos botoes do DualShock 4 (X, Quadrado, Triangulo, Circulo) desenhados em software.
#pragma once
#include "surface.h"

enum PadBtn {
    PADBTN_NONE = 0,
    PADBTN_CROSS = 1,
    PADBTN_CIRCLE = 2,
    PADBTN_SQUARE = 3,
    PADBTN_TRIANGLE = 4
};

// Converte nome ("cross","x","circle","o","square","triangle"...) em PadBtn.
PadBtn padbtn_from_name(const char* name);
const char* padbtn_name_pt(PadBtn b);
uint32_t padbtn_color(PadBtn b);

// Desenha o botao centrado em (cx,cy) com raio r (pixels). glow 0..1 = destaque (botao pressionado).
void padicon_draw(Surface* s, float cx, float cy, float r, PadBtn b, float glow);
