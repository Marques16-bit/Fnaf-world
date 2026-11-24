#include "text.h"
#include "font_data.h"

// Decodifica 1 caractere UTF-8; devolve o code point (ou '?' se fora de Latin-1).
static uint32_t utf8_next(const char** pp) {
    const unsigned char* p = (const unsigned char*)*pp;
    uint32_t c = p[0];
    if (c == 0) return 0;
    if (c < 0x80) {
        *pp += 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
        uint32_t cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
        *pp += 2;
        return cp;
    }
    // Sequencias maiores: pula e devolve '?'.
    int len = 1;
    if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    for (int i = 1; i < len && p[i]; ++i) {}
    *pp += len;
    return '?';
}

static int glyph_index(uint32_t cp) {
    if (cp >= 32 && cp <= 126) return (int)(cp - 32);
    if (cp >= 160 && cp <= 255) return 95 + (int)(cp - 160);
    return (int)('?' - 32);
}

int text_height(int scale) { return FONT_CELL_H * scale; }

int text_width(const char* s, int scale) {
    int best = 0, cur = 0;
    const char* p = s;
    for (;;) {
        uint32_t cp = utf8_next(&p);
        if (cp == 0) break;
        if (cp == '\n') {
            if (cur > best) best = cur;
            cur = 0;
            continue;
        }
        cur += FONT_CELL_W * scale;
    }
    return cur > best ? cur : best;
}

static void draw_glyph(Surface* s, int x, int y, int gi, uint32_t argb, int scale) {
    uint32_t base_a = argb >> 24;
    uint32_t rgb = argb & 0x00FFFFFFu;
    const uint8_t* g = FONT_ALPHA[gi];
    for (int gy = 0; gy < FONT_CELL_H; ++gy) {
        for (int gx = 0; gx < FONT_CELL_W; ++gx) {
            uint32_t a = g[gy * FONT_CELL_W + gx];
            if (!a) continue;
            a = (a * base_a + 127) / 255;
            if (!a) continue;
            uint32_t c = (a << 24) | rgb;
            for (int sy = 0; sy < scale; ++sy) {
                int py = y + gy * scale + sy;
                if (py < 0 || py >= s->h) continue;
                for (int sx = 0; sx < scale; ++sx) {
                    int px = x + gx * scale + sx;
                    if (px < 0 || px >= s->w) continue;
                    blend_over(&s->px[(size_t)py * s->stride + px], c);
                }
            }
        }
    }
}

void text_draw(Surface* s, int x, int y, const char* str, uint32_t argb, int scale) {
    if (scale < 1) scale = 1;
    int cx = x, cy = y;
    const char* p = str;
    for (;;) {
        uint32_t cp = utf8_next(&p);
        if (cp == 0) break;
        if (cp == '\n') {
            cx = x;
            cy += FONT_CELL_H * scale;
            continue;
        }
        if (cp != ' ') draw_glyph(s, cx, cy, glyph_index(cp), argb, scale);
        cx += FONT_CELL_W * scale;
    }
}

void text_draw_shadow(Surface* s, int x, int y, const char* str, uint32_t argb, int scale) {
    int o = scale < 1 ? 1 : scale;
    text_draw(s, x + o, y + o, str, 0xC0000000u, scale);
    text_draw(s, x, y, str, argb, scale);
}
