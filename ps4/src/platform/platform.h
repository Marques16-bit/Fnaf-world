// Interface entre o nucleo (portavel) e a plataforma (PS4 real ou host de teste).
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace plat {

// Mascaras de botoes (mesmos valores do ScePad da Sony).
enum PadMask : uint32_t {
    PAD_L3 = 0x00000002u,
    PAD_R3 = 0x00000004u,
    PAD_OPTIONS = 0x00000008u,
    PAD_UP = 0x00000010u,
    PAD_RIGHT = 0x00000020u,
    PAD_DOWN = 0x00000040u,
    PAD_LEFT = 0x00000080u,
    PAD_L2 = 0x00000100u,
    PAD_R2 = 0x00000200u,
    PAD_L1 = 0x00000400u,
    PAD_R1 = 0x00000800u,
    PAD_TRIANGLE = 0x00001000u,
    PAD_CIRCLE = 0x00002000u,
    PAD_CROSS = 0x00004000u,
    PAD_SQUARE = 0x00008000u,
    PAD_TOUCHPAD = 0x00100000u
};

struct PadState {
    uint32_t buttons;
    uint8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    bool connected;
};

// Abre video (framebuffer width x height, ARGB) e controle. Retorna false se falhar.
bool init(int width, int height);
// Buffer onde desenhar o proximo quadro (width*height pixels 0xAARRGGBB).
uint32_t* back_buffer();
// Mostra o quadro desenhado e passa para o proximo buffer.
void present();
// Le o controle. Retorna false se nao ha controle.
bool poll_pad(PadState* out);
// Memoria grande, cacheavel, para imagens e cenas. Retorna 0 se nao conseguiu.
void* alloc_scratch(size_t bytes);
// Pasta dos assets, terminada em '/'. Ex.: "/app0/assets/".
const char* asset_root();
// Log (printf-like). No PS4 grava em /data/fnafworld/log.txt.
void log(const char* fmt, ...);
// Milissegundos desde o inicio.
uint64_t ticks_ms();

}  // namespace plat
