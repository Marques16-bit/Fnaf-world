// Alocador "bump": uma unica area grande, sem malloc/free no meio do jogo.
#pragma once
#include "types.h"

struct Arena {
    uint8_t* base;
    size_t size;
    size_t used;
};

static inline void arena_init(Arena* a, void* mem, size_t size) {
    a->base = (uint8_t*)mem;
    a->size = size;
    a->used = 0;
}

static inline void* arena_alloc(Arena* a, size_t bytes, size_t align) {
    if (!a->base) return 0;
    size_t cur = (size_t)(a->base + a->used);
    size_t pad = (align - (cur % align)) % align;
    if (a->used + pad + bytes > a->size) return 0;
    void* p = a->base + a->used + pad;
    a->used += pad + bytes;
    return p;
}
