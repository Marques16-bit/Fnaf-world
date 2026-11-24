#include "frame.h"
#include "textparse.h"

bool frame_parse(const char* text, Frame* f) {
    f->name[0] = 0;
    f->w = 1280;
    f->h = 720;
    f->nlayers = 0;
    f->nhs = 0;

    TextReader r;
    tr_init(&r, text);
    char line[256], t0[32], t1[64], t2[32], t3[32], t4[32], t5[32], t6[32];
    while (tr_next_line(&r, line, sizeof(line))) {
        const char* c = line;
        if (!tok_next(&c, t0, sizeof(t0))) continue;
        if (t0[0] == 'f') {  // frame nome w h
            int w, h;
            if (tok_next(&c, t1, sizeof(t1)) && tok_next(&c, t2, sizeof(t2)) && tok_next(&c, t3, sizeof(t3)) &&
                tok_to_int(t2, &w) && tok_to_int(t3, &h) && w > 0 && h > 0) {
                str_copy(f->name, sizeof(f->name), t1);
                f->w = w;
                f->h = h;
            }
        } else if (t0[0] == 'l') {  // layer img x y
            if (f->nlayers >= FRAME_MAX_LAYERS) continue;
            if (!tok_next(&c, t1, sizeof(t1)) || !tok_next(&c, t2, sizeof(t2)) || !tok_next(&c, t3, sizeof(t3))) continue;
            int img = LAYER_IMAGE_TITLE, x, y;
            if (!(t1[0] == 't' && t1[1] == 'i')) {
                if (!tok_to_int(t1, &img)) continue;
            }
            if (!tok_to_int(t2, &x) || !tok_to_int(t3, &y)) continue;
            Layer* l = &f->layers[f->nlayers++];
            l->image = img;
            l->x = x;
            l->y = y;
        } else if (t0[0] == 'h') {  // hotspot label x y w h botao
            if (f->nhs >= FRAME_MAX_HOTSPOTS) continue;
            if (!tok_next(&c, t1, sizeof(t1)) || !tok_next(&c, t2, sizeof(t2)) || !tok_next(&c, t3, sizeof(t3)) ||
                !tok_next(&c, t4, sizeof(t4)) || !tok_next(&c, t5, sizeof(t5)) || !tok_next(&c, t6, sizeof(t6)))
                continue;
            int x, y, w, h;
            if (!tok_to_int(t2, &x) || !tok_to_int(t3, &y) || !tok_to_int(t4, &w) || !tok_to_int(t5, &h)) continue;
            PadBtn b = padbtn_from_name(t6);
            if (b == PADBTN_NONE) continue;
            Hotspot* hs = &f->hs[f->nhs++];
            str_copy(hs->label, sizeof(hs->label), t1);
            // '_' vira espaco para exibir
            for (char* q = hs->label; *q; ++q)
                if (*q == '_') *q = ' ';
            hs->x = x;
            hs->y = y;
            hs->w = w;
            hs->h = h;
            hs->btn = b;
        }
    }
    return f->nlayers > 0;
}
