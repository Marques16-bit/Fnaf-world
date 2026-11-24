// Mini-tokenizador de texto por linhas (sem STL, sem sscanf).
#pragma once
#include "types.h"

struct TextReader {
    const char* p;
};

static inline void tr_init(TextReader* r, const char* text) { r->p = text; }

// Le a proxima linha nao vazia e que nao seja comentario (#). Copia em line. Retorna false no fim.
bool tr_next_line(TextReader* r, char* line, size_t cap);

// Pega o proximo token separado por espaco/tab de *cursor (avanca o cursor). Retorna false se acabou.
bool tok_next(const char** cursor, char* out, size_t cap);

// Converte token em inteiro (aceita sinal). Retorna false se invalido.
bool tok_to_int(const char* tok, int* out);
