#include "fileio.h"
#include <stdio.h>

bool file_read_all(const char* path, uint8_t* dst, size_t cap, size_t* got) {
    if (got) *got = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t total = 0;
    while (total < cap) {
        size_t n = fread(dst + total, 1, cap - total, f);
        if (n == 0) break;
        total += n;
    }
    // Se ainda ha dados, o arquivo nao coube no buffer.
    int extra = fgetc(f);
    fclose(f);
    if (extra != EOF) return false;
    if (got) *got = total;
    return true;
}

bool file_read_text(const char* path, char* dst, size_t cap) {
    if (cap < 2) return false;
    size_t got = 0;
    if (!file_read_all(path, (uint8_t*)dst, cap - 1, &got)) return false;
    dst[got] = 0;
    return true;
}
