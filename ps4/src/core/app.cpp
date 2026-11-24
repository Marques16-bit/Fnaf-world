#include "app.h"
#include "fwi.h"
#include "fileio.h"
#include "text.h"
#include <string.h>
#include <stdio.h>

static const uint32_t COL_TEXT = 0xFFFFFFFFu;
static const uint32_t COL_DIM = 0xFFB8C0D0u;
static const uint32_t COL_WARN = 0xFFFFB347u;
static const uint32_t COL_ERR = 0xFFFF6B6Bu;
static const uint32_t COL_OK = 0xFF7CFC9Au;

static void set_status(App* a, const char* msg) {
    str_copy(a->status, sizeof(a->status), msg);
    plat::log("[app] %s", msg);
}

static void build_path(char* out, size_t cap, const char* rel) {
    snprintf(out, cap, "%s%s", plat::asset_root(), rel);
}

static void update_transform(App* a) {
    float sx = (float)a->fbw / (float)a->frame.w;
    float sy = (float)a->fbh / (float)a->frame.h;
    a->scale = sx < sy ? sx : sy;
    a->off_x = (int)(((float)a->fbw - (float)a->frame.w * a->scale) * 0.5f);
    a->off_y = (int)(((float)a->fbh - (float)a->frame.h * a->scale) * 0.5f);
}

// Imagem de teste (quando nao ha assets): degrade + listras + moldura.
static void draw_test_pattern(Surface* s) {
    for (int y = 0; y < s->h; ++y) {
        for (int x = 0; x < s->w; ++x) {
            uint32_t r = (uint32_t)(20 + (x * 40) / s->w);
            uint32_t g = (uint32_t)(20 + (y * 50) / s->h);
            uint32_t b = (uint32_t)(40 + ((x + y) * 60) / (s->w + s->h));
            if (((x + y) / 48) % 2 == 0) {
                r += 6;
                g += 6;
                b += 6;
            }
            s->px[(size_t)y * s->stride + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    surf_rect_outline(s, 8, 8, s->w - 16, s->h - 16, 4, 0xFF3A4A7Au);
}

static bool load_and_decode(App* a, int image_index, int* out_w, int* out_h) {
    if (image_index < 0 || image_index >= a->man.nimg) {
        char m[160];
        snprintf(m, sizeof(m), "Indice de imagem invalido: %d", image_index);
        set_status(a, m);
        return false;
    }
    char path[200];
    build_path(path, sizeof(path), a->man.img[image_index].file);
    size_t got = 0;
    if (!file_read_all(path, a->filebuf, a->filecap, &got)) {
        char m[320];
        snprintf(m, sizeof(m), "Nao consegui ler %s", path);
        set_status(a, m);
        return false;
    }
    int w = 0, h = 0;
    if (!fwi_decode(a->filebuf, got, a->tmp.px, (size_t)a->tmp.w * (size_t)a->tmp.h, &w, &h)) {
        char m[320];
        snprintf(m, sizeof(m), "Imagem invalida (FWI): %s", a->man.img[image_index].file);
        set_status(a, m);
        return false;
    }
    *out_w = w;
    *out_h = h;
    return true;
}

static void compose_scene(App* a) {
    a->need_compose = false;
    surf_fill(&a->scene, 0xFF0B0B10u);
    if (!a->man_ok || !a->frame_ok) {
        draw_test_pattern(&a->scene);
        return;
    }
    update_transform(a);
    a->status[0] = 0;
    for (int i = 0; i < a->frame.nlayers; ++i) {
        const Layer* l = &a->frame.layers[i];
        int img = l->image == LAYER_IMAGE_TITLE ? a->sel : l->image;
        int iw = 0, ih = 0;
        if (!load_and_decode(a, img, &iw, &ih)) continue;
        Surface src = surface_make(a->tmp.px, iw, ih);  // a imagem decodificada ocupa o inicio do buffer tmp
        int dx = a->off_x + (int)((float)l->x * a->scale + 0.5f);
        int dy = a->off_y + (int)((float)l->y * a->scale + 0.5f);
        int dw = (int)((float)iw * a->scale + 0.5f);
        int dh = (int)((float)ih * a->scale + 0.5f);
        surf_blit_scaled(&a->scene, dx, dy, dw, dh, &src);
    }
}

void app_init(App* a, Arena* arena, int fbw, int fbh) {
    memset(a, 0, sizeof(*a));
    a->fbw = fbw;
    a->fbh = fbh;
    a->overlay = true;
    a->scale = 1.0f;

    // Superficies principais. Se a memoria faltar, o app ainda desenha texto de erro direto no framebuffer.
    uint32_t* scene_px = (uint32_t*)arena_alloc(arena, (size_t)fbw * fbh * 4, 64);
    const int tmp_w = 2048, tmp_h = 1152;
    uint32_t* tmp_px = (uint32_t*)arena_alloc(arena, (size_t)tmp_w * tmp_h * 4, 64);
    a->filecap = (size_t)tmp_w * tmp_h * 4 + (size_t)tmp_w * tmp_h / 64 + 4096;
    a->filebuf = (uint8_t*)arena_alloc(arena, a->filecap, 64);
    if (!scene_px || !tmp_px || !a->filebuf) {
        plat::log("[app] arena insuficiente (usada %u de %u)", (unsigned)arena->used, (unsigned)arena->size);
        a->scene = surface_make(0, 0, 0);
        set_status(a, "Memoria insuficiente para iniciar o leitor.");
        return;
    }
    a->scene = surface_make(scene_px, fbw, fbh);
    a->tmp = surface_make(tmp_px, tmp_w, tmp_h);

    // Manifesto
    static char text[16384];
    char path[200];
    build_path(path, sizeof(path), "manifest.txt");
    if (!file_read_text(path, text, sizeof(text))) {
        char m[320];
        snprintf(m, sizeof(m), "Manifesto nao encontrado: %s", path);
        set_status(a, m);
    } else if (!manifest_parse(text, &a->man)) {
        set_status(a, "Manifesto invalido (manifest.txt).");
    } else {
        a->man_ok = true;
        a->sel = a->man.title;
        plat::log("[app] manifesto ok: %d imagens, janela %dx%d, placeholder=%d", a->man.nimg, a->man.win_w,
                  a->man.win_h, a->man.placeholder ? 1 : 0);
        // Frame
        static char ftext[16384];
        build_path(path, sizeof(path), a->man.frame_file[0] ? a->man.frame_file : "frames/title.frame");
        if (!file_read_text(path, ftext, sizeof(ftext))) {
            char m[320];
            snprintf(m, sizeof(m), "Frame nao encontrado: %s", path);
            set_status(a, m);
        } else if (!frame_parse(ftext, &a->frame)) {
            set_status(a, "Frame invalido (title.frame).");
        } else {
            a->frame_ok = true;
            plat::log("[app] frame '%s' %dx%d: %d camadas, %d areas", a->frame.name, a->frame.w, a->frame.h,
                      a->frame.nlayers, a->frame.nhs);
        }
    }
    update_transform(a);
    compose_scene(a);
}

static void trigger_hotspots(App* a, PadBtn b) {
    if (!a->frame_ok) return;
    for (int i = 0; i < a->frame.nhs; ++i) {
        if (a->frame.hs[i].btn == b) {
            a->flash[i] = 20;
            snprintf(a->last_action, sizeof(a->last_action), "Acionado: %s  (%s)", a->frame.hs[i].label,
                     padbtn_name_pt(b));
            a->last_action_timer = 120;
            plat::log("[app] %s", a->last_action);
        }
    }
}

static void change_sel(App* a, int delta) {
    if (!a->man_ok || a->man.nimg <= 0) return;
    int n = a->man.nimg;
    a->sel = ((a->sel + delta) % n + n) % n;
    a->need_compose = true;
}

void app_update(App* a, const plat::PadState* pad) {
    a->frame_no++;
    a->pad_ok = pad && pad->connected;
    uint32_t cur = pad ? pad->buttons : 0;
    uint32_t pressed = cur & ~a->prev_buttons;

    if (pressed & plat::PAD_RIGHT) change_sel(a, 1);
    if (pressed & plat::PAD_LEFT) change_sel(a, -1);
    if (pressed & plat::PAD_R1) change_sel(a, 10);
    if (pressed & plat::PAD_L1) change_sel(a, -10);
    // auto-repeticao ao segurar
    a->hold_right = (cur & plat::PAD_RIGHT) ? a->hold_right + 1 : 0;
    a->hold_left = (cur & plat::PAD_LEFT) ? a->hold_left + 1 : 0;
    if (a->hold_right > 25 && (a->hold_right % 6) == 0) change_sel(a, 1);
    if (a->hold_left > 25 && (a->hold_left % 6) == 0) change_sel(a, -1);

    if (pressed & plat::PAD_OPTIONS) a->overlay = !a->overlay;
    if (pressed & plat::PAD_CROSS) trigger_hotspots(a, PADBTN_CROSS);
    if (pressed & plat::PAD_CIRCLE) trigger_hotspots(a, PADBTN_CIRCLE);
    if (pressed & plat::PAD_SQUARE) trigger_hotspots(a, PADBTN_SQUARE);
    if (pressed & plat::PAD_TRIANGLE) trigger_hotspots(a, PADBTN_TRIANGLE);

    for (int i = 0; i < FRAME_MAX_HOTSPOTS; ++i)
        if (a->flash[i] > 0) a->flash[i]--;
    if (a->last_action_timer > 0) a->last_action_timer--;
    a->prev_buttons = cur;

    if (a->need_compose && a->scene.px) compose_scene(a);
}

static void draw_overlay(App* a, Surface* fb) {
    for (int i = 0; i < a->frame.nhs; ++i) {
        const Hotspot* h = &a->frame.hs[i];
        int x = a->off_x + (int)((float)h->x * a->scale);
        int y = a->off_y + (int)((float)h->y * a->scale);
        int w = (int)((float)h->w * a->scale);
        int hh = (int)((float)h->h * a->scale);
        uint32_t col = padbtn_color(h->btn);
        uint32_t rgb = col & 0x00FFFFFFu;
        float f = a->flash[i] > 0 ? (float)a->flash[i] / 20.0f : 0.0f;

        surf_fill_rect(fb, x, y, w, hh, ((uint32_t)(0x26 + (int)(f * 90)) << 24) | rgb);
        surf_rect_outline(fb, x, y, w, hh, f > 0 ? 4 : 2, 0xFF000000u | rgb);

        float r = (float)imin(imax(hh / 3, 22), 38);
        if (f > 0) r *= 1.0f + 0.25f * f;
        float cx = (float)x + (float)w * 0.5f;
        float cy = (float)y + (float)hh * 0.5f;
        padicon_draw(fb, cx, cy, r, h->btn, f);

        // rotulo abaixo do icone (ou abaixo da area se nao couber)
        int tw = text_width(h->label, 1);
        int ty = (int)(cy + r + 6.0f);
        if (ty + text_height(1) > y + hh) ty = y + hh + 4;
        int tx = (int)(cx - (float)tw * 0.5f);
        text_draw_shadow(fb, tx, ty, h->label, COL_TEXT, 1);
    }
}

static void draw_hud(App* a, Surface* fb) {
    char line[256];
    int pad = 14;
    int lh = text_height(1) + 2;
    int lines = 4;
    surf_fill_rect(fb, 12, 12, 980, lines * lh + 2 * pad, 0xB0000000u);
    int x = 12 + pad, y = 12 + pad;

    text_draw(fb, x, y, "FNaF World PS4 - Etapa 1 (leitor básico)", COL_TEXT, 1);
    y += lh;
    if (a->man_ok) {
        const ImageEntry* e = &a->man.img[a->sel];
        snprintf(line, sizeof(line), "Imagem %d/%d   %s   %dx%d", a->sel + 1, a->man.nimg, e->file, e->w, e->h);
        text_draw(fb, x, y, line, COL_TEXT, 1);
        y += lh;
        snprintf(line, sizeof(line), "Original: %s", e->orig);
        text_draw(fb, x, y, line, COL_DIM, 1);
        y += lh;
        if (a->man.placeholder)
            text_draw(fb, x, y, "ASSETS NÃO EXTRAÍDOS - modo de teste (veja o relatório do GitHub Actions)", COL_WARN, 1);
        else {
            snprintf(line, sizeof(line), "Overlay: %s   Controle: %s   Quadro: %u", a->overlay ? "LIGADO" : "desligado",
                     a->pad_ok ? "OK" : "não detectado", (unsigned)a->frame_no);
            text_draw(fb, x, y, line, a->pad_ok ? COL_OK : COL_WARN, 1);
        }
    } else {
        text_draw(fb, x, y, "SEM ASSETS NO PACOTE", COL_ERR, 1);
        y += lh;
        text_draw(fb, x, y, a->status, COL_WARN, 1);
    }

    // barra de dicas
    const char* hint = "D-pad < > ou L1/R1: trocar imagem    OPTIONS: overlay    X / O / [] / ^ : acionar áreas";
    int hh = text_height(1) + 16;
    surf_fill_rect(fb, 0, fb->h - hh, fb->w, hh, 0xC0000000u);
    text_draw(fb, 24, fb->h - hh + 8, hint, COL_DIM, 1);

    if (a->last_action_timer > 0) {
        int tw = text_width(a->last_action, 2);
        int tx = fb->w - tw - 40, ty = 28;
        surf_fill_rect(fb, tx - 16, ty - 8, tw + 32, text_height(2) + 16, 0xC0101830u);
        text_draw(fb, tx, ty, a->last_action, COL_OK, 2);
    }
    if (a->status[0] && a->man_ok) {
        text_draw_shadow(fb, 24, fb->h - hh - text_height(1) - 8, a->status, COL_ERR, 1);
    }
}

void app_draw(App* a, Surface* fb) {
    if (a->scene.px) {
        surf_copy(fb, &a->scene);
    } else {
        draw_test_pattern(fb);
    }
    if (a->overlay && a->frame_ok) draw_overlay(a, fb);
    draw_hud(a, fb);
}
