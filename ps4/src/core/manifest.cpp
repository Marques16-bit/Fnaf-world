#include "manifest.h"
#include "textparse.h"

bool manifest_parse(const char* text, Manifest* m) {
    m->win_w = 1280;
    m->win_h = 720;
    m->placeholder = false;
    m->nimg = 0;
    m->title = 0;
    m->frame_file[0] = 0;

    TextReader r;
    tr_init(&r, text);
    char line[256], t0[64], t1[96], t2[96], t3[32], t4[32], t5[96];
    bool header = false;
    while (tr_next_line(&r, line, sizeof(line))) {
        const char* c = line;
        if (!tok_next(&c, t0, sizeof(t0))) continue;
        if (!header) {
            if (t0[0] != 'f' || t0[1] != 'w') return false;  // "fwmanifest"
            header = true;
            continue;
        }
        if (t0[0] == 'w' && t0[1] == 'i') {  // window W H
            int w, h;
            if (tok_next(&c, t1, sizeof(t1)) && tok_next(&c, t2, sizeof(t2)) && tok_to_int(t1, &w) &&
                tok_to_int(t2, &h) && w > 0 && h > 0) {
                m->win_w = w;
                m->win_h = h;
            }
        } else if (t0[0] == 'p') {  // placeholder 0|1
            int v;
            if (tok_next(&c, t1, sizeof(t1)) && tok_to_int(t1, &v)) m->placeholder = v != 0;
        } else if (t0[0] == 'i') {  // image idx file w h orig
            int idx, w, h;
            if (!tok_next(&c, t1, sizeof(t1)) || !tok_next(&c, t2, sizeof(t2)) || !tok_next(&c, t3, sizeof(t3)) ||
                !tok_next(&c, t4, sizeof(t4)))
                continue;
            if (!tok_to_int(t1, &idx) || !tok_to_int(t3, &w) || !tok_to_int(t4, &h)) continue;
            if (idx < 0 || idx >= MAN_MAX_IMAGES) continue;
            t5[0] = 0;
            tok_next(&c, t5, sizeof(t5));
            ImageEntry* e = &m->img[idx];
            str_copy(e->file, sizeof(e->file), t2);
            e->w = w;
            e->h = h;
            str_copy(e->orig, sizeof(e->orig), t5);
            if (idx + 1 > m->nimg) m->nimg = idx + 1;
        } else if (t0[0] == 't') {  // title idx
            int v;
            if (tok_next(&c, t1, sizeof(t1)) && tok_to_int(t1, &v)) m->title = v;
        } else if (t0[0] == 'f') {  // frame title path
            if (tok_next(&c, t1, sizeof(t1)) && tok_next(&c, t2, sizeof(t2))) str_copy(m->frame_file, sizeof(m->frame_file), t2);
        }
    }
    if (!header) return false;
    if (m->title < 0 || m->title >= m->nimg) m->title = 0;
    return m->nimg > 0;
}
