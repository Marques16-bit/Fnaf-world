// Texto com fonte bitmap embutida (font_data.h). Aceita UTF-8 (Latin-1 coberto).
#pragma once
#include "surface.h"

// Largura em pixels do texto (maior linha) com o fator de escala inteiro.
int text_width(const char* s, int scale);
int text_height(int scale);
// Desenha texto. '\n' quebra linha.
void text_draw(Surface* s, int x, int y, const char* str, uint32_t argb, int scale);
// Texto com sombra escura (melhor leitura sobre imagens).
void text_draw_shadow(Surface* s, int x, int y, const char* str, uint32_t argb, int scale);
