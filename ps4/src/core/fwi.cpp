#include "fwi.h"

static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool fwi_header(const uint8_t* data, size_t size, int* w, int* h) {
    if (size < FWI_HEADER_SIZE) return false;
    if (data[0] != 'F' || data[1] != 'W' || data[2] != 'I' || data[3] != '1') return false;
    uint32_t ww = rd32(data + 4), hh = rd32(data + 8);
    if (ww == 0 || hh == 0 || ww > 8192 || hh > 8192) return false;
    if (w) *w = (int)ww;
    if (h) *h = (int)hh;
    return true;
}

bool fwi_decode(const uint8_t* data, size_t size, uint32_t* out_px, size_t cap_px, int* w, int* h) {
    int iw = 0, ih = 0;
    if (!fwi_header(data, size, &iw, &ih)) return false;
    size_t npx = (size_t)iw * (size_t)ih;
    if (npx > cap_px) return false;
    uint32_t flags = rd32(data + 12);
    uint32_t plen = rd32(data + 16);
    if ((size_t)FWI_HEADER_SIZE + plen > size) return false;
    const uint8_t* p = data + FWI_HEADER_SIZE;
    const uint8_t* end = p + plen;

    if (!(flags & FWI_FLAG_RLE)) {
        if ((size_t)plen < npx * 4) return false;
        for (size_t i = 0; i < npx; ++i) out_px[i] = rd32(p + i * 4);
    } else {
        size_t o = 0;
        while (o < npx) {
            if (p >= end) return false;
            uint32_t c = *p++;
            if (c < 128) {
                size_t n = (size_t)c + 1;
                if ((size_t)(end - p) < n * 4 || o + n > npx) return false;
                for (size_t i = 0; i < n; ++i, p += 4) out_px[o + i] = rd32(p);
                o += n;
            } else {
                size_t n = (size_t)(c - 128) + 2;
                if ((size_t)(end - p) < 4 || o + n > npx) return false;
                uint32_t v = rd32(p);
                p += 4;
                for (size_t i = 0; i < n; ++i) out_px[o + i] = v;
                o += n;
            }
        }
    }
    if (w) *w = iw;
    if (h) *h = ih;
    return true;
}
