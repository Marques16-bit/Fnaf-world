// Plataforma PS4 (OpenOrbis): video por framebuffer em software (sceVideoOut) + controle (ScePad).
//
// Nao usamos os headers do toolchain de proposito: declaramos aqui so as funcoes da Sony que precisamos
// (assinaturas oficiais). Os simbolos vem das bibliotecas stub do toolchain (-lkernel -lSceVideoOut ...).
//
// Como o video funciona aqui:
//   * O app desenha num buffer comum e CACHEADO ("render"), na memoria normal da CPU (rapida para ler e escrever).
//   * A cada quadro, copiamos esse buffer para um dos 2 buffers de EXIBICAO, que ficam em memoria
//     write-combining (GARLIC). O controlador de video do PS4 le a memoria direto, sem passar pelo cache da CPU;
//     por isso desenhar direto numa memoria cacheada podia mostrar pedacos velhos do quadro (o "pisca").
//   * Depois da copia damos um sfence, enviamos o flip e ESPERAMOS ele ser exibido antes de reaproveitar o outro buffer.
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
const int MEM_WB_ONION = 0;    // cacheada pela CPU
const int MEM_WC_GARLIC = 3;   // write-combining, lida direto da RAM (ideal para o video)
const int PROT_CPU_RW = 0x03;
const int PROT_CPU_GPU_RW = 0x33;
const size_t MB2 = 2u * 1024u * 1024u;

int g_video = -1;
int g_pad = -1;
int g_user = 0;
int g_w = 0, g_h = 0;
size_t g_bytes = 0;            // bytes de um quadro (w*h*4)
uint32_t* g_disp[2] = {0, 0};  // buffers de exibicao (registrados no VideoOut)
uint32_t* g_render = 0;        // buffer onde o app desenha
int g_cur = 0;                 // buffer de exibicao livre para receber o proximo quadro
uint64_t g_start_us = 0;
uint8_t g_pad_raw[512] __attribute__((aligned(16)));
uint8_t g_flip_status[256] __attribute__((aligned(16)));
int g_pad_retry = 0;
bool g_log_ready = false;
uint64_t g_flip_base = 0;     // contador de flips ao abrir o video
uint64_t g_submitted = 0;     // flips enviados por nos
bool g_flip_poll_ok = true;   // false = o contador nao se comportou; usa tempo fixo
int g_present_calls = 0;
uint64_t g_last_present_us = 0;

size_t round_up(size_t v, size_t a) { return (v + a - 1) / a * a; }
uint64_t now_us() { return sceKernelGetProcessTime(); }

// Le o contador de flips concluidos (primeiro campo de SceVideoOutFlipStatus).
bool read_flip_count(uint64_t* out) {
    memset(g_flip_status, 0, sizeof(g_flip_status));
    if (sceVideoOutGetFlipStatus(g_video, g_flip_status) != 0) return false;
    memcpy(out, g_flip_status, 8);
    return true;
}

// Espera o ultimo flip enviado ser EXIBIDO. So depois disso o buffer antigo pode ser reescrito
// sem aparecer na tela.
void wait_flip_done() {
    if (g_flip_poll_ok) {
        for (int i = 0; i < 200; ++i) {
            uint64_t c = 0;
            if (!read_flip_count(&c)) break;
            if (c >= g_flip_base + g_submitted) return;
            sceKernelUsleep(500);
        }
        plat::log("[ps4] contador de flips nao avancou; usando espera fixa de 14 ms");
        g_flip_poll_ok = false;
    }
    sceKernelUsleep(14000);
}

// Aloca memoria direta mapeada na CPU. Tenta o tipo pedido e o alternativo; *used_type diz qual funcionou.
void* map_direct(size_t bytes, int prot, int type_first, int type_second, int* used_type) {
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
        if (used_type) *used_type = types[i];
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

uint64_t ticks_ms() { return (now_us() - g_start_us) / 1000u; }

void sleep_ms(int ms) {
    if (ms > 0) sceKernelUsleep((unsigned int)ms * 1000u);
}

const char* asset_root() { return "/app0/assets/"; }

void* alloc_scratch(size_t bytes) {
    int used = -1;
    void* p = map_direct(bytes, PROT_CPU_RW, MEM_WB_ONION, MEM_WC_GARLIC, &used);
    if (!p) {
        log("[ps4] scratch via direct memory falhou; tentando malloc(%u)", (unsigned)bytes);
        p = malloc(bytes);
    }
    return p;
}

bool init(int width, int height) {
    g_start_us = now_us();
    g_w = width;
    g_h = height;
    g_bytes = (size_t)width * (size_t)height * 4;

    int r = sceUserServiceInitialize(0);
    log("[ps4] sceUserServiceInitialize -> 0x%x", (unsigned)r);
    r = sceUserServiceGetInitialUser(&g_user);
    log("[ps4] sceUserServiceGetInitialUser -> 0x%x (user %d)", (unsigned)r, g_user);

    // --- video ---
    g_video = sceVideoOutOpen(SYSTEM_USER_ID, 0 /*MAIN*/, 0, 0);
    log("[ps4] sceVideoOutOpen -> %d", g_video);
    if (g_video < 0) return false;

    // 2 buffers de exibicao em memoria write-combining (cada um alinhado a 2 MB)
    size_t buf_bytes = round_up(g_bytes, MB2);
    int disp_type = -1;
    uint8_t* mem = (uint8_t*)map_direct(buf_bytes * 2, PROT_CPU_GPU_RW, MEM_WC_GARLIC, MEM_WB_ONION, &disp_type);
    if (!mem) {
        log("[ps4] nao consegui memoria para os framebuffers");
        return false;
    }
    log("[ps4] buffers de exibicao em memoria tipo %d (%s)", disp_type, disp_type == MEM_WC_GARLIC ? "GARLIC/write-combining" : "ONION/cacheada");
    memset(mem, 0, buf_bytes * 2);
    g_disp[0] = (uint32_t*)mem;
    g_disp[1] = (uint32_t*)(mem + buf_bytes);

    // buffer de desenho comum (cacheado): leitura/escrita rapida para o app
    int render_type = -1;
    g_render = (uint32_t*)map_direct(g_bytes, PROT_CPU_RW, MEM_WB_ONION, MEM_WC_GARLIC, &render_type);
    if (!g_render) {
        log("[ps4] direct memory para o buffer de desenho falhou; usando malloc");
        g_render = (uint32_t*)malloc(g_bytes);
    }
    if (!g_render) return false;
    memset(g_render, 0, g_bytes);

    uint8_t attr[128] __attribute__((aligned(16)));
    memset(attr, 0, sizeof(attr));
    sceVideoOutSetBufferAttribute(attr, PIXEL_FMT_A8R8G8B8_SRGB, TILING_LINEAR, ASPECT_16_9, (uint32_t)width,
                                  (uint32_t)height, (uint32_t)width);
    void* addrs[2] = {g_disp[0], g_disp[1]};
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

uint32_t* back_buffer() { return g_render; }

void present() {
    uint64_t t0 = now_us();
    // 1) copia o quadro pronto para o buffer de exibicao que NAO esta na tela
    memcpy(g_disp[g_cur], g_render, g_bytes);
    __asm__ volatile("sfence" ::: "memory");  // garante que os writes write-combining chegaram na RAM
    uint64_t t1 = now_us();

    // 2) pede para exibir no proximo vsync
    int r = -1;
    for (int tries = 0; tries < 200 && r != 0; ++tries) {
        r = sceVideoOutSubmitFlip(g_video, g_cur, FLIP_MODE_VSYNC, 0);
        if (r != 0) sceKernelUsleep(1000);
    }
    if (r == 0) g_submitted++;

    // 3) o OUTRO buffer ainda esta na tela ate o flip acontecer: so liberamos ele depois
    wait_flip_done();
    uint64_t t2 = now_us();
    g_cur ^= 1;
    g_present_calls++;

    if (g_present_calls == 5 || g_present_calls == 60 || g_present_calls == 300) {
        log("[ps4] quadro %d: copia=%u us, espera do flip=%u us, desde o quadro anterior=%u us", g_present_calls,
            (unsigned)(t1 - t0), (unsigned)(t2 - t1), (unsigned)(g_last_present_us ? t2 - g_last_present_us : 0));
    }
    g_last_present_us = t2;
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
