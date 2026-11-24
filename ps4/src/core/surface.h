// Superficie de pixels 0xAARRGGBB + primitivas de desenho em software.
#pragma once
#include "types.h"

struct Surface {
    uint32_t* px;
    int w;
    int h;
    int stride;  // em pixels
};

static inline Surface surface_make(uint32_t* px, int w, int h) {
    Surface s;
    s.px = px;
    s.w = w;
    s.h = h;
    s.stride = w;
    return s;
}

// Composicao "source over" sobre destino opaco.
void blend_over(uint32_t* dst, uint32_t src_argb);

void surf_fill(Surface* s, uint32_t argb);
// Copia src para dst (mesmo tamanho; copia o minimo comum).
void surf_copy(Surface* dst, const Surface* src);
// Retangulo cheio com alpha.
void surf_fill_rect(Surface* s, int x, int y, int w, int h, uint32_t argb);
// Contorno de retangulo com espessura t.
void surf_rect_outline(Surface* s, int x, int y, int w, int h, int t, uint32_t argb);
// Escala bilinear de src para o retangulo (dx,dy,dw,dh) de dst, com alpha.
void surf_blit_scaled(Surface* dst, int dx, int dy, int dw, int dh, const Surface* src);
