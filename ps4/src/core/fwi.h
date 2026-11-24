// Formato FWI1: imagem RGBA propria, gerada por tools/pack_assets.py.
//
//   offset 0  : "FWI1"
//   offset 4  : u32 largura
//   offset 8  : u32 altura
//   offset 12 : u32 flags (bit0 = RLE)
//   offset 16 : u32 tamanho do payload em bytes
//   offset 20 : payload
//
// Cada pixel = u32 little-endian no formato 0xAARRGGBB (igual ao framebuffer do PS4).
// RLE estilo PackBits sobre pixels de 32 bits:
//   controle c <= 127 : copia os proximos (c+1) pixels literalmente
//   controle c >= 128 : repete o proximo pixel (c-128+2) vezes
#pragma once
#include "types.h"

#define FWI_HEADER_SIZE 20
#define FWI_FLAG_RLE 1u

// Le so o cabecalho. Retorna false se invalido.
bool fwi_header(const uint8_t* data, size_t size, int* w, int* h);

// Decodifica para out_px (capacidade cap_px pixels). Retorna false se invalido.
bool fwi_decode(const uint8_t* data, size_t size, uint32_t* out_px, size_t cap_px, int* w, int* h);
