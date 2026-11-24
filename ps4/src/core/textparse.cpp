#include "textparse.h"

bool tr_next_line(TextReader* r, char* line, size_t cap) {
    for (;;) {
        if (!r->p || !*r->p) return false;
        // pula espacos iniciais
        const char* s = r->p;
        while (*s == ' ' || *s == '\t' || *s == '\r') ++s;
        size_t n = 0;
        const char* e = s;
        while (*e && *e != '\n') ++e;
        // copia a linha
        for (const char* q = s; q < e && n + 1 < cap; ++q) line[n++] = *q;
        line[n] = 0;
        // tira \r e espacos do final
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) line[--n] = 0;
        r->p = *e ? e + 1 : e;
        if (n == 0 || line[0] == '#') continue;
        return true;
    }
}

bool tok_next(const char** cursor, char* out, size_t cap) {
    const char* s = *cursor;
    while (*s == ' ' || *s == '\t') ++s;
    if (!*s) {
        *cursor = s;
        return false;
    }
    size_t n = 0;
    while (*s && *s != ' ' && *s != '\t') {
        if (n + 1 < cap) out[n++] = *s;
        ++s;
    }
    out[n] = 0;
    *cursor = s;
    return true;
}

bool tok_to_int(const char* tok, int* out) {
    int sign = 1;
    if (*tok == '-') {
        sign = -1;
        ++tok;
    } else if (*tok == '+') {
        ++tok;
    }
    if (!*tok) return false;
    long v = 0;
    while (*tok) {
        if (*tok < '0' || *tok > '9') return false;
        v = v * 10 + (*tok - '0');
        if (v > 1000000000L) return false;
        ++tok;
    }
    *out = (int)(v * sign);
    return true;
}
