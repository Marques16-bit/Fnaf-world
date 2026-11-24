#include "padicons.h"

static bool name_eq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

PadBtn padbtn_from_name(const char* n) {
    if (name_eq(n, "cross") || name_eq(n, "x") || name_eq(n, "xis")) return PADBTN_CROSS;
    if (name_eq(n, "circle") || name_eq(n, "o") || name_eq(n, "circulo")) return PADBTN_CIRCLE;
    if (name_eq(n, "square") || name_eq(n, "quadrado")) return PADBTN_SQUARE;
    if (name_eq(n, "triangle") || name_eq(n, "triangulo")) return PADBTN_TRIANGLE;
    return PADBTN_NONE;
}

const char* padbtn_name_pt(PadBtn b) {
    switch (b) {
        case PADBTN_CROSS: return "X";
        case PADBTN_CIRCLE: return "Circulo";
        case PADBTN_SQUARE: return "Quadrado";
        case PADBTN_TRIANGLE: return "Triangulo";
        default: return "?";
    }
}

uint32_t padbtn_color(PadBtn b) {
    switch (b) {
        case PADBTN_CROSS: return 0xFF6F9BFFu;     // azul
        case PADBTN_CIRCLE: return 0xFFF0555Fu;    // vermelho
        case PADBTN_SQUARE: return 0xFFE78AC8u;    // rosa
        case PADBTN_TRIANGLE: return 0xFF3DD9B0u;  // verde-agua
        default: return 0xFFCCCCCCu;
    }
}

static float seg_dist(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay;
    float wx = px - ax, wy = py - ay;
    float len2 = vx * vx + vy * vy;
    float t = len2 > 0 ? (wx * vx + wy * vy) / len2 : 0;
    t = fclampf(t, 0.0f, 1.0f);
    float dx = px - (ax + vx * t), dy = py - (ay + vy * t);
    return fsqrt_(dx * dx + dy * dy);
}

// Distancia ate o "esqueleto" do simbolo (em unidades de r, y para baixo).
static float symbol_dist(PadBtn b, float ux, float uy) {
    switch (b) {
        case PADBTN_CROSS: {
            float d1 = seg_dist(ux, uy, -0.36f, -0.36f, 0.36f, 0.36f);
            float d2 = seg_dist(ux, uy, -0.36f, 0.36f, 0.36f, -0.36f);
            return fminf_(d1, d2);
        }
        case PADBTN_CIRCLE: {
            float d = fsqrt_(ux * ux + uy * uy);
            return fabsf_(d - 0.40f);
        }
        case PADBTN_SQUARE: {
            const float hs = 0.34f;
            float qx = fabsf_(ux) - hs, qy = fabsf_(uy) - hs;
            float ox = fmaxf_(qx, 0.0f), oy = fmaxf_(qy, 0.0f);
            float outside = fsqrt_(ox * ox + oy * oy);
            float inside = fminf_(fmaxf_(qx, qy), 0.0f);
            return fabsf_(outside + inside);
        }
        case PADBTN_TRIANGLE: {
            float d1 = seg_dist(ux, uy, 0.0f, -0.42f, 0.40f, 0.28f);
            float d2 = seg_dist(ux, uy, 0.40f, 0.28f, -0.40f, 0.28f);
            float d3 = seg_dist(ux, uy, -0.40f, 0.28f, 0.0f, -0.42f);
            return fminf_(d1, fminf_(d2, d3));
        }
        default: return 10.0f;
    }
}

void padicon_draw(Surface* s, float cx, float cy, float r, PadBtn b, float glow) {
    if (r < 4.0f) return;
    uint32_t col = padbtn_color(b);
    uint32_t rgb = col & 0x00FFFFFFu;
    float halo = glow > 0.0f ? r * 0.7f : 0.0f;
    float ext = r + halo + 2.0f;
    int x0 = imax((int)(cx - ext), 0), x1 = imin((int)(cx + ext) + 1, s->w);
    int y0 = imax((int)(cy - ext), 0), y1 = imin((int)(cy + ext) + 1, s->h);
    float half_thick = fmaxf_(0.085f * r, 1.4f) / r;  // em unidades de r
    float ring_w = fmaxf_(2.5f, r * 0.07f);

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            float d = fsqrt_(dx * dx + dy * dy);
            uint32_t* dst = &s->px[(size_t)y * s->stride + x];

            // Halo de destaque (botao acionado).
            if (halo > 0.0f && d > r) {
                float t = 1.0f - (d - r) / halo;
                if (t > 0.0f) {
                    uint32_t a = (uint32_t)(glow * 150.0f * t * t);
                    if (a > 0) blend_over(dst, (a << 24) | rgb);
                }
            }
            // Disco escuro.
            float cov = fclampf(r + 0.5f - d, 0.0f, 1.0f);
            if (cov > 0.0f) {
                uint32_t a = (uint32_t)(cov * (glow > 0.0f ? 245.0f : 225.0f));
                uint32_t fill = glow > 0.0f ? 0x2A3350u : 0x141824u;
                blend_over(dst, (a << 24) | fill);
            }
            // Aro colorido.
            float ring = fclampf(ring_w * 0.5f + 0.5f - fabsf_(d - (r - ring_w * 0.5f)), 0.0f, 1.0f);
            if (ring > 0.0f && cov > 0.0f) {
                uint32_t a = (uint32_t)(ring * 255.0f);
                blend_over(dst, (a << 24) | rgb);
            }
            // Simbolo.
            if (d < r) {
                float sd = symbol_dist(b, dx / r, dy / r) * r;  // em pixels
                float sc = fclampf(half_thick * r + 0.5f - sd, 0.0f, 1.0f);
                if (sc > 0.0f) {
                    uint32_t a = (uint32_t)(sc * 255.0f);
                    uint32_t sym = glow > 0.0f ? 0xFFFFFFu : rgb;
                    blend_over(dst, (a << 24) | sym);
                }
            }
        }
    }
}
