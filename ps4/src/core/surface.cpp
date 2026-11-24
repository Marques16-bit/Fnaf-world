#include "surface.h"
#include <string.h>

static inline uint32_t div255(uint32_t x) { return ((x + 128u) * 257u) >> 16; }

void blend_over(uint32_t* dst, uint32_t s) {
    uint32_t sa = s >> 24;
    if (sa == 0) return;
    if (sa == 255) {
        *dst = s;
        return;
    }
    uint32_t d = *dst;
    uint32_t ia = 255u - sa;
    uint32_t r = div255(((s >> 16) & 255u) * sa + ((d >> 16) & 255u) * ia);
    uint32_t g = div255(((s >> 8) & 255u) * sa + ((d >> 8) & 255u) * ia);
    uint32_t b = div255((s & 255u) * sa + (d & 255u) * ia);
    *dst = 0xFF000000u | (r << 16) | (g << 8) | b;
}

void surf_fill(Surface* s, uint32_t argb) {
    for (int y = 0; y < s->h; ++y) {
        uint32_t* row = s->px + (size_t)y * s->stride;
        for (int x = 0; x < s->w; ++x) row[x] = argb;
    }
}

void surf_copy(Surface* dst, const Surface* src) {
    int w = imin(dst->w, src->w), h = imin(dst->h, src->h);
    for (int y = 0; y < h; ++y)
        memcpy(dst->px + (size_t)y * dst->stride, src->px + (size_t)y * src->stride, (size_t)w * 4);
}

void surf_fill_rect(Surface* s, int x, int y, int w, int h, uint32_t argb) {
    int x0 = imax(x, 0), y0 = imax(y, 0), x1 = imin(x + w, s->w), y1 = imin(y + h, s->h);
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t a = argb >> 24;
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = s->px + (size_t)yy * s->stride;
        if (a == 255) {
            for (int xx = x0; xx < x1; ++xx) row[xx] = argb;
        } else {
            for (int xx = x0; xx < x1; ++xx) blend_over(&row[xx], argb);
        }
    }
}

void surf_rect_outline(Surface* s, int x, int y, int w, int h, int t, uint32_t argb) {
    if (w <= 0 || h <= 0 || t <= 0) return;
    surf_fill_rect(s, x, y, w, t, argb);
    surf_fill_rect(s, x, y + h - t, w, t, argb);
    surf_fill_rect(s, x, y + t, t, h - 2 * t, argb);
    surf_fill_rect(s, x + w - t, y + t, t, h - 2 * t, argb);
}

#define MAX_SCALED_W 4096

void surf_blit_scaled(Surface* dst, int dx, int dy, int dw, int dh, const Surface* src) {
    if (dw <= 0 || dh <= 0 || src->w <= 0 || src->h <= 0) return;
    int x0 = imax(dx, 0), y0 = imax(dy, 0), x1 = imin(dx + dw, dst->w), y1 = imin(dy + dh, dst->h);
    if (x0 >= x1 || y0 >= y1) return;
    if (x1 - x0 > MAX_SCALED_W) x1 = x0 + MAX_SCALED_W;

    static int xi[MAX_SCALED_W];
    static uint8_t xw[MAX_SCALED_W];
    const int64_t maxfx = (int64_t)(src->w - 1) << 16;
    const int64_t maxfy = (int64_t)(src->h - 1) << 16;

    for (int x = x0; x < x1; ++x) {
        int64_t fx = ((((int64_t)(2 * (x - dx) + 1) * src->w) << 15) / dw) - 32768;
        if (fx < 0) fx = 0;
        if (fx > maxfx) fx = maxfx;
        xi[x - x0] = (int)(fx >> 16);
        xw[x - x0] = (uint8_t)((fx >> 8) & 255);
    }

    for (int y = y0; y < y1; ++y) {
        int64_t fy = ((((int64_t)(2 * (y - dy) + 1) * src->h) << 15) / dh) - 32768;
        if (fy < 0) fy = 0;
        if (fy > maxfy) fy = maxfy;
        int iy0 = (int)(fy >> 16);
        int iy1 = imin(iy0 + 1, src->h - 1);
        uint32_t wy = (uint32_t)((fy >> 8) & 255);
        const uint32_t* r0 = src->px + (size_t)iy0 * src->stride;
        const uint32_t* r1 = src->px + (size_t)iy1 * src->stride;
        uint32_t* out = dst->px + (size_t)y * dst->stride;

        for (int x = x0; x < x1; ++x) {
            int ix0 = xi[x - x0];
            int ix1 = imin(ix0 + 1, src->w - 1);
            uint32_t wx = xw[x - x0];
            uint32_t p00 = r0[ix0], p10 = r0[ix1], p01 = r1[ix0], p11 = r1[ix1];
            uint32_t w00 = (256 - wx) * (256 - wy), w10 = wx * (256 - wy);
            uint32_t w01 = (256 - wx) * wy, w11 = wx * wy;  // soma = 65536

            if (((p00 & p10 & p01 & p11) >> 24) == 255u) {
                // Caminho rapido: tudo opaco.
                uint32_t r = (((p00 >> 16) & 255) * w00 + ((p10 >> 16) & 255) * w10 + ((p01 >> 16) & 255) * w01 +
                              ((p11 >> 16) & 255) * w11) >> 16;
                uint32_t g = (((p00 >> 8) & 255) * w00 + ((p10 >> 8) & 255) * w10 + ((p01 >> 8) & 255) * w01 +
                              ((p11 >> 8) & 255) * w11) >> 16;
                uint32_t b = ((p00 & 255) * w00 + (p10 & 255) * w10 + (p01 & 255) * w01 + (p11 & 255) * w11) >> 16;
                out[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            } else {
                // Caminho com alpha: interpola em alpha pre-multiplicado.
                uint64_t a00 = p00 >> 24, a10 = p10 >> 24, a01 = p01 >> 24, a11 = p11 >> 24;
                uint64_t asum = a00 * w00 + a10 * w10 + a01 * w01 + a11 * w11;  // alpha * 65536
                if (asum < 65536u / 2) continue;
                uint64_t rs = (uint64_t)((p00 >> 16) & 255) * a00 * w00 + (uint64_t)((p10 >> 16) & 255) * a10 * w10 +
                              (uint64_t)((p01 >> 16) & 255) * a01 * w01 + (uint64_t)((p11 >> 16) & 255) * a11 * w11;
                uint64_t gs = (uint64_t)((p00 >> 8) & 255) * a00 * w00 + (uint64_t)((p10 >> 8) & 255) * a10 * w10 +
                              (uint64_t)((p01 >> 8) & 255) * a01 * w01 + (uint64_t)((p11 >> 8) & 255) * a11 * w11;
                uint64_t bs = (uint64_t)(p00 & 255) * a00 * w00 + (uint64_t)(p10 & 255) * a10 * w10 +
                              (uint64_t)(p01 & 255) * a01 * w01 + (uint64_t)(p11 & 255) * a11 * w11;
                uint32_t a = (uint32_t)(asum >> 16);
                if (a > 255) a = 255;
                uint32_t r = (uint32_t)(rs / asum), g = (uint32_t)(gs / asum), b = (uint32_t)(bs / asum);
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                blend_over(&out[x], (a << 24) | (r << 16) | (g << 8) | b);
            }
        }
    }
}
