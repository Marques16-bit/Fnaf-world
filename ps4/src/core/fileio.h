// Leitura de arquivos via stdio (funciona no PS4/musl e no host).
#pragma once
#include "types.h"

// Retorna true e preenche *got com os bytes lidos. Nao le alem de cap.
bool file_read_all(const char* path, uint8_t* dst, size_t cap, size_t* got);
// Le um arquivo texto (termina com \0). Retorna false se nao coube ou nao abriu.
bool file_read_text(const char* path, char* dst, size_t cap);
