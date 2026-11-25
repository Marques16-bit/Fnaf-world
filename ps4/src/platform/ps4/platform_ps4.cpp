// Plataforma PS4 (OpenOrbis): video por framebuffer em software (sceVideoOut) + controle (ScePad).
//
// Nao usamos os headers do toolchain de proposito: declaramos aqui so as funcoes da Sony que precisamos
// (assinaturas oficiais). Os simbolos vem das bibliotecas stub do toolchain (-lkernel -lSceVideoOut ...).
#include "../platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

extern "C" {
// libkernel
int sceKernelUsleep(unsigned int microseconds);
size_t sceKernelGetDirectMemorySize(void);
int sceKernelAllocateDirectMemory(int64_t searchStart, int64_t searchEnd, size_t length, size_t alignment, int type,
                                  int64_t* physAddrOut);
int sceKernelMapDirectMemory(void** addr, size_t length, int prot, int flags, int64_t directMemoryStart,
                             size_t maxPageSize);
uint64_t sceKernelGetProcessTime(void);  // microssegundos
// libSceUserService
int sceUserServiceInitialize(const void* params);
int sceUserServiceGetInitialUser(int* userId);
// libSceVideoOut
int sceVideoOutOpen(int userId, int type, int index, const void* param);
void sceVideoOutSetBufferAttribute(void* attr, uint32_t pixelFormat, uint32_t tilingMode, uint32_t aspectRatio,
                                   uint32_t width, uint32_t height, uint32_t pitchInPixel);
int sceVideoOutRegisterBuffers(int handle, int startIndex, void* const* addresses, int bufferNum, const void* attr);
int sceVideoOutSetFlipRate(int handle, int rate);
int sceVideoOutSubmitFlip(int handle, int bufferIndex, uint32_t flipMode, int64_t flipArg);
int sceVideoOutGetFlipStatus(int handle, void* status);
// libScePad
int scePadInit(void);
int scePadOpen(int userId, int type, int index, const void* param);
int scePadReadState(int handle, void* data);
}

namespace {

const int SYSTEM_USER_ID = 255;          // SCE_USER_SERVICE_USER_ID_SYSTEM
const uint32_t PIXEL_FMT_A8R8G8B8_SRGB = 0x80000000u;
const uint32_t TILING_LINEAR = 1;
const uint32_t ASPECT_16_9 = 0;
const uint32_t FLIP_MODE_VSYNC = 1;
const int MEM_WB_ONION = 0;
const int MEM_WC_GARLIC = 3;
const int PROT_CPU_RW = 0x03;
const int PROT_CPU_GPU_RW = 0x33;
const size_t MB2 = 2u * 1024u * 1024u;

int g_video = -1;
int g_pad = -1;
int g_user = 0;
int g_w = 0, g_h = 0;
uint32_t* g_buf[2] = {0, 0};
int g_cur = 0;
uint64_t g_start_us = 0;
uint8_t g_pad_raw[512] __attribute__((aligned(16)));
uint8_t g_flip_status[256] __attribute__((aligned(16)));
int g_pad_retry = 0;
bool g_log_ready = false;
uint64_t g_flip_base = 0;     // contador de flips ao abrir o video
uint64_t g_submitted = 0;     // flips enviados por nos
bool g_flip_poll_ok = true;   // false = o contador nao se comportou; usa tempo fixo
int g_present_calls = 0;

size_t round_up(size_t v, size_t a) { return (v + a - 1) / a * a; }

// Le o contador de flips concluidos (primeiro campo de SceVideoOutFlipStatus).
bool read_flip_count(uint64_t* out) {
    memset(g_flip_status, 0, sizeof(g_flip_status));
    if (sceVideoOutGetFlipStatus(g_video, g_flip_status) != 0) return false;
    memcpy(out, g_flip_status, 8);
    return true;
}

// Espera o ultimo flip enviado ser EXIBIDO. So depois disso o buffer antigo pode ser redesenhado
// sem aparecer na tela (era isso que fazia o overlay "piscar").
void wait_flip_done() {
    if (g_flip_poll_ok) {
        for (int i = 0; i < 200; ++i) {
            uint64_t c = 0;
            if (!read_flip_count(&c)) break;
            if (c >= g_flip_base + g_submitted) {
                if (g_present_calls < 4) plat::log("[ps4] flip %d concluido (contador=%d base=%d)", g_present_calls, (int)c, (int)g_flip_base);
                return;
            }
            sceKernelUsleep(500);
        }
        plat::log("[ps4] contador de flips nao avancou; usando espera fixa de 14 ms");
        g_flip_poll_ok = false;
    }
    sceKernelUsleep(14000);
}

// Aloca memoria direta mapeada na CPU. Tenta o tipo pedido e o alternativo.
void* map_direct(size_t bytes, int prot, int type_first, int type_second) {
    size_t len = round_up(bytes, MB2);
    int types[2] = {type_first, type_second};
    for (int i = 0; i < 2; ++i) {
        int64_t phys = 0;
        int r = sceKernelAllocateDirectMemory(0, (int64_t)sceKernelGetDirectMemorySize(), len, MB2, types[i], &phys);
        if (r != 0) {
            plat::log("[ps4] AllocateDirectMemory(tipo %d, %u bytes) falhou: 0x%x", types[i], (unsigned)len, (unsigned)r);
            continue;
        }
        void* addr = 0;
        r = sceKernelMapDirectMemory(&addr, len, prot, 0, phys, MB2);
        if (r != 0 || !addr) {
            plat::log("[ps4] MapDirectMemory falhou: 0x%x", (unsigned)r);
            continue;
        }
        return addr;
    }
    return 0;
}

}  // namespace

namespace plat {

void log(const char* fmt, ...) {
    char line[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (!g_log_ready) {
        mkdir("/data/fnafworld", 0777);
        g_log_ready = true;
    }
    FILE* f = fopen("/data/fnafworld/log.txt", "ab");
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
    printf("%s\n", line);
}

uint64_t ticks_ms() { return (sceKernelGetProcessTime() - g_start_us) / 1000u; }

const char* asset_root() { return "/app0/assets/"; }

void* alloc_scratch(size_t bytes) {
    void* p = map_direct(bytes, PROT_CPU_RW, MEM_WB_ONION, MEM_WC_GARLIC);
    if (!p) {
        log("[ps4] scratch via direct memory falhou; tentando malloc(%u)", (unsigned)bytes);
        p = malloc(bytes);
    }
    return p;
}

bool init(int width, int height) {
    g_start_us = sceKernelGetProcessTime();
    g_w = width;
    g_h = height;

    int r = sceUserServiceInitialize(0);
    log("[ps4] sceUserServiceInitialize -> 0x%x", (unsigned)r);
    r = sceUserServiceGetInitialUser(&g_user);
    log("[ps4] sceUserServiceGetInitialUser -> 0x%x (user %d)", (unsigned)r, g_user);

    // --- video ---
    g_video = sceVideoOutOpen(SYSTEM_USER_ID, 0 /*MAIN*/, 0, 0);
    log("[ps4] sceVideoOutOpen -> %d", g_video);
    if (g_video < 0) return false;

    size_t buf_bytes = round_up((size_t)width * height * 4, MB2);
    uint8_t* mem = (uint8_t*)map_direct(buf_bytes * 2, PROT_CPU_GPU_RW, MEM_WB_ONION, MEM_WC_GARLIC);
    if (!mem) {
        log("[ps4] nao consegui memoria para os framebuffers");
        return false;
    }
    memset(mem, 0, buf_bytes * 2);
    g_buf[0] = (uint32_t*)mem;
    g_buf[1] = (uint32_t*)(mem + buf_bytes);

    uint8_t attr[128] __attribute__((aligned(16)));
    memset(attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(attr, PIXEL_FMT_A8R8G8B8_SRGB, TILING_LINEAR, ASPECT_16_9, (uint32_t)width,
                                  (uint32_t)height, (uint32_t)width);
    void* addrs[2] = {g_buf[0], g_buf[1]};
    r = sceVideoOutRegisterBuffers(g_video, 0, addrs, 2, attr);
    log("[ps4] sceVideoOutRegisterBuffers -> 0x%x", (unsigned)r);
    if (r != 0) return false;
    sceVideoOutSetFlipRate(g_video, 0);

    uint64_t base = 0;
    if (read_flip_count(&base)) g_flip_base = base;
    else g_flip_poll_ok = false;
    log("[ps4] contador de flips inicial = %d (poll %s)", (int)g_flip_base, g_flip_poll_ok ? "ok" : "indisponivel");

    // primeiro quadro (preto) para a tela acordar
    g_cur = 0;
    if (sceVideoOutSubmitFlip(g_video, 0, FLIP_MODE_VSYNC, 0) == 0) g_submitted++;
    wait_flip_done();
    g_cur = 1;

    // --- controle ---
    r = scePadInit();
    log("[ps4] scePadInit -> 0x%x", (unsigned)r);
    g_pad = scePadOpen(g_user, 0 /*STANDARD*/, 0, 0);
    log("[ps4] scePadOpen -> %d", g_pad);
    return true;
}

uint32_t* back_buffer() { return g_buf[g_cur]; }

void present() {
    int r = -1;
    for (int tries = 0; tries < 200 && r != 0; ++tries) {
        r = sceVideoOutSubmitFlip(g_video, g_cur, FLIP_MODE_VSYNC, 0);
        if (r != 0) sceKernelUsleep(1000);
    }
    if (r == 0) g_submitted++;
    // O buffer g_cur vai para a tela no proximo vsync. O OUTRO buffer ainda esta na tela ate o flip
    // acontecer, entao so liberamos ele para desenho depois que o flip terminar.
    wait_flip_done();
    g_present_calls++;
    g_cur ^= 1;
}

bool poll_pad(PadState* out) {
    memset(out, 0, sizeof(*out));
    if (g_pad < 0) {
        // tenta reabrir de tempos em tempos (controle ligado depois do app)
        if ((g_pad_retry++ % 120) == 0) g_pad = scePadOpen(g_user, 0, 0, 0);
        if (g_pad < 0) return false;
    }
    memset(g_pad_raw, 0, sizeof(g_pad_raw));
    int r = scePadReadState(g_pad, g_pad_raw);
    if (r != 0) return false;
    // ScePadData: buttons(u32) @0, leftStick x,y @4,5, rightStick x,y @6,7, l2,r2 @8,9
    memcpy(&out->buttons, g_pad_raw, 4);
    out->lx = g_pad_raw[4];
    out->ly = g_pad_raw[5];
    out->rx = g_pad_raw[6];
    out->ry = g_pad_raw[7];
    out->l2 = g_pad_raw[8];
    out->r2 = g_pad_raw[9];
    out->connected = true;
    return true;
}

}  // namespace plat
