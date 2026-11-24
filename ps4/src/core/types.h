// Tipos e utilitarios minimos (sem STL, para manter o build do PS4 simples).
#pragma once
#include <stdint.h>
#include <stddef.h>

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float fclampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float fabsf_(float v) { return v < 0 ? -v : v; }
static inline float fmaxf_(float a, float b) { return a > b ? a : b; }
static inline float fminf_(float a, float b) { return a < b ? a : b; }
static inline float fsqrt_(float v) { return __builtin_sqrtf(v); }

// Cores no formato 0xAARRGGBB (mesma ordem de memoria do framebuffer A8R8G8B8 do PS4).
static inline uint32_t make_argb(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

// Copia segura de string para buffer fixo.
static inline void str_copy(char* dst, size_t cap, const char* src) {
    if (cap == 0) return;
    size_t i = 0;
    for (; i + 1 < cap && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}
