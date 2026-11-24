// Ponto de entrada (PS4). Para o host de teste veja tests/host_main.cpp.
#include "core/app.h"
#include "platform/platform.h"

#define FB_W 1920
#define FB_H 1080
#define SCRATCH_BYTES (48u * 1024u * 1024u)

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    plat::log("=== FNaF World PS4 - Etapa 1 iniciando ===");
    if (!plat::init(FB_W, FB_H)) {
        plat::log("[main] falha ao iniciar video/controle");
        for (;;) {}
    }

    static Arena arena;
    void* mem = plat::alloc_scratch(SCRATCH_BYTES);
    arena_init(&arena, mem, mem ? SCRATCH_BYTES : 0);

    static App app;
    app_init(&app, &arena, FB_W, FB_H);

    for (;;) {
        plat::PadState pad = {};
        plat::poll_pad(&pad);
        app_update(&app, &pad);
        Surface fb = surface_make(plat::back_buffer(), FB_W, FB_H);
        app_draw(&app, &fb);
        plat::present();
    }
    return 0;
}
