#!/usr/bin/env python3
"""Gera ps4/src/core/font_data.h (fonte bitmap embutida) a partir da DejaVu Sans Mono.

Uso (so e necessario se quiser regenerar a fonte; o arquivo gerado ja vem no repositorio):
    python3 tools/gen_font.py /caminho/DejaVuSansMono.ttf ps4/src/core/font_data.h
"""
import sys
from PIL import Image, ImageDraw, ImageFont

CW, CH = 12, 24
SIZE = 19


def main():
    font_path = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "ps4/src/core/font_data.h"
    font = ImageFont.truetype(font_path, SIZE)
    codes = list(range(32, 127)) + list(range(160, 256))
    glyphs = []
    for c in codes:
        img = Image.new("L", (CW, CH), 0)
        d = ImageDraw.Draw(img)
        ch = chr(c)
        w = font.getlength(ch)
        x = max(0, int(round((CW - w) / 2)))
        d.text((x, 0), ch, font=font, fill=255)
        glyphs.append(list(img.tobytes()))
    with open(out_path, "w") as f:
        f.write("// Gerado por tools/gen_font.py (DejaVu Sans Mono, licenca Bitstream Vera/DejaVu - uso livre).\n")
        f.write("// Cada glifo: 12x24 pixels, 1 byte de alpha por pixel. Codigos: 32..126 e 160..255 (Latin-1).\n")
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write("#define FONT_CELL_W 12\n#define FONT_CELL_H 24\n#define FONT_GLYPH_COUNT %d\n\n" % len(glyphs))
        f.write("static const uint8_t FONT_ALPHA[FONT_GLYPH_COUNT][FONT_CELL_W * FONT_CELL_H] = {\n")
        for gi, g in enumerate(glyphs):
            f.write("  /* %d */ {" % codes[gi])
            f.write(",".join(str(v) for v in g))
            f.write("},\n")
        f.write("};\n")
    print("ok:", out_path, len(glyphs), "glifos")


if __name__ == "__main__":
    main()
