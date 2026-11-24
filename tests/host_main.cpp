// Teste no PC/Linux: roda o leitor sem PS4 e grava o ultimo quadro em .ppm.
//   uso: host_test <pasta_assets> <saida.ppm> [frames] [roteiro]
// roteiro: lista "quadro:botao" separada por virgulas. Ex.: "5:right,10:cross,12:options"
//          botoes: right left up down l1 r1 cross circle square triangle options
#include "../ps4/src/core/app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_set_asset_root(const char* root);
void host_set_buttons(uint32_t mask);
const uint32_t* host_framebuffer();

static uint32_t mask_from_name(const char* n) {
    struct { const char* n; uint32_t m; } t[] = {
        {"right", plat::PAD_RIGHT}, {"left", plat::PAD_LEFT}, {"up", plat::PAD_UP}, {"down", plat::PAD_DOWN},
        {"l1", plat::PAD_L1}, {"r1", plat::PAD_R1}, {"cross", plat::PAD_CROSS}, {"circle", plat::PAD_CIRCLE},
        {"square", plat::PAD_SQUARE}, {"triangle", plat::PAD_TRIANGLE}, {"options", plat::PAD_OPTIONS}};
    for (auto& e : t)
        if (strcmp(e.n, n) == 0) return e.m;
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "uso: %s <pasta_assets> <saida.ppm> [frames] [roteiro]\n", argv[0]);
        return 2;
    }
    host_set_asset_root(argv[1]);
    int frames = argc > 3 ? atoi(argv[3]) : 30;
    const char* script = argc > 4 ? argv[4] : "";

    if (!plat::init(1920, 1080)) return 1;
    static Arena arena;
    size_t sz = 48u * 1024u * 1024u;
    arena_init(&arena, plat::alloc_scratch(sz), sz);
    static App app;
    app_init(&app, &arena, 1920, 1080);

    for (int f = 0; f < frames; ++f) {
        uint32_t mask = 0;
        // procura "f:botao" no roteiro; o botao fica pressionado so naquele quadro
        const char* p = script;
        while (*p) {
            int qf = atoi(p);
            const char* colon = strchr(p, ':');
            if (!colon) break;
            char name[32] = {0};
            const char* e = colon + 1;
            size_t n = 0;
            while (*e && *e != ',' && n + 1 < sizeof(name)) name[n++] = *e++;
            if (qf == f) mask |= mask_from_name(name);
            p = *e == ',' ? e + 1 : e;
        }
        host_set_buttons(mask);
        plat::PadState pad;
        plat::poll_pad(&pad);
        app_update(&app, &pad);
        Surface fb = surface_make(plat::back_buffer(), 1920, 1080);
        app_draw(&app, &fb);
        plat::present();
    }

    FILE* o = fopen(argv[2], "wb");
    if (!o) return 1;
    fprintf(o, "P6\n1920 1080\n255\n");
    const uint32_t* px = host_framebuffer();
    for (int i = 0; i < 1920 * 1080; ++i) {
        unsigned char rgb[3] = {(unsigned char)(px[i] >> 16), (unsigned char)(px[i] >> 8), (unsigned char)px[i]};
        fwrite(rgb, 1, 3, o);
    }
    fclose(o);
    fprintf(stderr, "ok: %d quadros, ppm em %s\n", frames, argv[2]);
    return 0;
}
