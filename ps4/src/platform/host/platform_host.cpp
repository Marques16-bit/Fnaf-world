// Plataforma "host" (Linux/PC) usada SO para testar o nucleo sem um PS4.
// Nao entra no build do PS4.
#include "../platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

namespace {
const int W = 1920, H = 1080;
uint32_t* g_fb = 0;
uint32_t g_script_buttons = 0;
char g_root[512] = "assets/";
uint64_t g_ms = 0;
}  // namespace

// API extra, so do host (declarada em tests/host_main.cpp)
void host_set_asset_root(const char* root) {
    snprintf(g_root, sizeof(g_root), "%s", root);
    size_t n = strlen(g_root);
    if (n && g_root[n - 1] != '/' && n + 1 < sizeof(g_root)) {
        g_root[n] = '/';
        g_root[n + 1] = 0;
    }
}
void host_set_buttons(uint32_t mask) { g_script_buttons = mask; }
const uint32_t* host_framebuffer() { return g_fb; }

namespace plat {
bool init(int width, int height) {
    if (width != W || height != H) return false;
    g_fb = (uint32_t*)calloc((size_t)W * H, 4);
    return g_fb != 0;
}
uint32_t* back_buffer() { return g_fb; }
void present() { g_ms += 16; }
bool poll_pad(PadState* out) {
    memset(out, 0, sizeof(*out));
    out->buttons = g_script_buttons;
    out->connected = true;
    return true;
}
void* alloc_scratch(size_t bytes) { return calloc(1, bytes); }
const char* asset_root() { return g_root; }
void log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}
uint64_t ticks_ms() { return g_ms; }
void sleep_ms(int) {}
}  // namespace plat
