#!/usr/bin/env python3
"""pack_assets.py - transforma as imagens extraidas do FNaFWorld.exe nos assets do PS4.

Entrada : uma pasta com imagens (PNG/BMP/JPG) em qualquer profundidade (saida do extrator).
Saida   : ps4/assets/
              manifest.txt          lista de imagens + imagem de titulo + arquivo do frame
              img/0000.fwi ...      imagens candidatas no formato FWI1 (RGBA com RLE)
              frames/title.frame    camadas + areas clicaveis (hotspots) da tela inicial
          relatorio/ (opcional)
              contato_*.png         folhas de contato (miniaturas numeradas) para voce escolher
              imagens.txt           lista de TODAS as imagens encontradas (nome, tamanho)
              pack_report.txt       resumo

Se nao houver nenhuma imagem (extracao falhou), gera uma imagem de teste ("modo placeholder")
para o app do PS4 continuar compilando e mostrando algo na tela.

Uso:
    python tools/pack_assets.py --dump work/dump --out ps4/assets --report work/relatorio
    python tools/pack_assets.py --placeholder --out ps4/assets
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
from collections import Counter

import numpy as np
from PIL import Image, ImageDraw, ImageFont

IMG_EXT = {".png", ".bmp", ".jpg", ".jpeg", ".tga", ".gif"}
DEFAULT_HOTSPOTS = """# x y w h botao rotulo   (coordenadas no espaco da janela do jogo, ex.: 1280x720)
# botoes: cross (X), circle (Circulo), square (Quadrado), triangle (Triangulo)
 80 500 250 90 cross Jogar
360 500 250 90 circle Voltar
640 500 250 90 square Extras
920 500 250 90 triangle Opcoes
"""


# ----------------------------------------------------------------------------- FWI1
def argb_array(img):
    """PIL -> array uint32 (h*w) no formato 0xAARRGGBB."""
    arr = np.asarray(img.convert("RGBA"), dtype=np.uint32)
    r, g, b, a = arr[..., 0], arr[..., 1], arr[..., 2], arr[..., 3]
    return ((a << 24) | (r << 16) | (g << 8) | b).astype("<u4").reshape(-1)


def rle_encode(px):
    """PackBits sobre pixels de 32 bits (veja ps4/src/core/fwi.h)."""
    n = int(px.size)
    out = bytearray()
    if n == 0:
        return bytes(out)
    change = (np.flatnonzero(px[1:] != px[:-1]) + 1).tolist()
    starts = [0] + change
    ends = change + [n]

    def flush(lo, hi):
        i = lo
        while i < hi:
            k = min(128, hi - i)
            out.append(k - 1)
            out.extend(px[i:i + k].tobytes())
            i += k

    lit_begin = 0
    for s, e in zip(starts, ends):
        ln = e - s
        if ln >= 3:
            flush(lit_begin, s)
            v = px[s:s + 1].tobytes()
            rem = ln
            while rem > 0:
                k = min(129, rem)
                if rem - k == 1:
                    k -= 1
                out.append(128 + k - 2)
                out.extend(v)
                rem -= k
            lit_begin = e
    flush(lit_begin, n)
    return bytes(out)


def fwi_encode(img):
    w, h = img.size
    px = argb_array(img)
    rle = rle_encode(px)
    raw = px.tobytes()
    if len(rle) < len(raw):
        flags, payload = 1, rle
    else:
        flags, payload = 0, raw
    return b"FWI1" + struct.pack("<IIII", w, h, flags, len(payload)) + payload


def fwi_decode(data):
    """Decodificador de referencia (usado nos testes)."""
    assert data[:4] == b"FWI1"
    w, h, flags, plen = struct.unpack("<IIII", data[4:20])
    p = data[20:20 + plen]
    if not flags & 1:
        return w, h, np.frombuffer(p, dtype="<u4").copy()
    out = []
    i = 0
    while i < len(p):
        c = p[i]
        i += 1
        if c < 128:
            n = c + 1
            out.append(np.frombuffer(p[i:i + 4 * n], dtype="<u4"))
            i += 4 * n
        else:
            n = c - 128 + 2
            out.append(np.full(n, np.frombuffer(p[i:i + 4], dtype="<u4")[0], dtype="<u4"))
            i += 4
    return w, h, np.concatenate(out)


# ----------------------------------------------------------------------------- utilidades
def natural_key(s):
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", s)]


def load_font(size):
    for cand in (
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
    ):
        if os.path.exists(cand):
            try:
                return ImageFont.truetype(cand, size)
            except Exception:
                pass
    try:
        return ImageFont.load_default(size=size)
    except TypeError:
        return ImageFont.load_default()


def scan_images(dump):
    found = []
    for root, _dirs, files in os.walk(dump):
        for fn in files:
            if os.path.splitext(fn)[1].lower() not in IMG_EXT:
                continue
            path = os.path.join(root, fn)
            try:
                with Image.open(path) as im:
                    w, h = im.size
            except Exception:
                continue
            found.append({"path": path, "rel": os.path.relpath(path, dump).replace("\\", "/"), "w": w, "h": h})
    found.sort(key=lambda d: natural_key(d["rel"]))
    return found


def read_cfg(path):
    cfg = {"window": None, "max": None, "prefer": []}
    if not path or not os.path.exists(path):
        return cfg
    for raw in open(path, encoding="utf-8", errors="replace"):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        t = line.split()
        if t[0] == "window" and len(t) >= 3:
            cfg["window"] = (int(t[1]), int(t[2]))
        elif t[0] == "max_candidates" and len(t) >= 2:
            cfg["max"] = int(t[1])
        elif t[0] == "prefer" and len(t) >= 2:
            cfg["prefer"].append(" ".join(t[1:]).lower())
    return cfg


def detect_window(images):
    sizes = Counter((d["w"], d["h"]) for d in images if d["w"] >= 800 and abs(d["w"] / d["h"] - 16 / 9) < 0.03)
    if sizes:
        return sizes.most_common(1)[0][0]
    if images:
        big = max(images, key=lambda d: d["w"] * d["h"])
        return big["w"], big["h"]
    return 1280, 720


def pick_candidates(images, window, prefer, limit):
    W, H = window
    chosen, seen = [], set()

    def add(d):
        if d["path"] not in seen and len(chosen) < limit:
            seen.add(d["path"])
            chosen.append(d)

    for key in prefer:
        for d in images:
            if key in d["rel"].lower():
                add(d)
    for d in images:
        if d["w"] == W and d["h"] == H:
            add(d)
    if len(chosen) < limit:
        rest = sorted(images, key=lambda d: -d["w"] * d["h"])  # as maiores primeiro, de qualquer tamanho
        for d in rest:
            add(d)
    return chosen


def make_placeholder_image(W, H):
    arr = np.zeros((H, W, 4), dtype=np.uint8)
    yy, xx = np.mgrid[0:H, 0:W]
    arr[..., 0] = (30 + xx * 50 // W).astype(np.uint8)
    arr[..., 1] = (24 + yy * 40 // H).astype(np.uint8)
    arr[..., 2] = (60 + (xx + yy) * 60 // (W + H)).astype(np.uint8)
    arr[..., 3] = 255
    im = Image.fromarray(arr, "RGBA")
    d = ImageDraw.Draw(im)
    d.rectangle([20, 20, W - 21, H - 21], outline=(255, 190, 80, 255), width=6)
    f1, f2 = load_font(54), load_font(30)
    d.text((W // 2, H // 2 - 70), "FNAF WORLD - MODO DE TESTE", font=f1, fill=(255, 255, 255, 255), anchor="mm")
    d.text((W // 2, H // 2 + 10), "Os assets ainda nao foram extraidos do .exe", font=f2, fill=(255, 220, 160, 255), anchor="mm")
    d.text((W // 2, H // 2 + 60), "Rode o workflow '1 - Extrair assets' e depois recompile.", font=f2, fill=(220, 220, 220, 255), anchor="mm")
    return im


def contact_sheets(items, out_dir, prefix, per_sheet=20, cols=4, thumb=(320, 180)):
    os.makedirs(out_dir, exist_ok=True)
    font = load_font(20)
    tw, th = thumb
    rows = (per_sheet + cols - 1) // cols
    paths = []
    for si in range(0, len(items), per_sheet):
        chunk = items[si:si + per_sheet]
        sheet = Image.new("RGB", (cols * (tw + 8) + 8, rows * (th + 40) + 8), (24, 24, 30))
        d = ImageDraw.Draw(sheet)
        for i, (label, path) in enumerate(chunk):
            cx, cy = 8 + (i % cols) * (tw + 8), 8 + (i // cols) * (th + 40)
            try:
                with Image.open(path) as im:
                    im = im.convert("RGBA")
                    im.thumbnail(thumb)
                    bg = Image.new("RGBA", thumb, (50, 50, 60, 255))
                    bg.paste(im, ((tw - im.width) // 2, (th - im.height) // 2), im)
                    sheet.paste(bg.convert("RGB"), (cx, cy + 32))
            except Exception:
                pass
            d.text((cx, cy + 4), label, font=font, fill=(255, 235, 150))
        p = os.path.join(out_dir, "%s_%02d.png" % (prefix, si // per_sheet + 1))
        sheet.save(p)
        paths.append(p)
    return paths


def read_hotspots(path):
    text = DEFAULT_HOTSPOTS
    if path and os.path.exists(path):
        text = open(path, encoding="utf-8", errors="replace").read()
    out = []
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        t = line.split()
        if len(t) < 6:
            continue
        try:
            x, y, w, h = (int(v) for v in t[:4])
        except ValueError:
            continue
        btn = t[4].lower()
        label = "_".join(t[5:])
        label = re.sub(r"[^0-9A-Za-z_\-]", "", label) or "Area"
        out.append("hotspot %s %d %d %d %d %s" % (label, x, y, w, h, btn))
    return out


# ----------------------------------------------------------------------------- principal
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", help="pasta com as imagens extraidas")
    ap.add_argument("--out", default="ps4/assets")
    ap.add_argument("--report", default=None, help="pasta do relatorio (folhas de contato etc.)")
    ap.add_argument("--config", default="config", help="pasta com stage1.cfg e hotspots_title.txt")
    ap.add_argument("--max-candidates", type=int, default=None)
    ap.add_argument("--placeholder", action="store_true", help="ignora o dump e gera so a imagem de teste")
    ap.add_argument("--only-if-missing", action="store_true", help="so gera se manifest.txt ainda nao existir")
    args = ap.parse_args()

    out = args.out
    if args.only_if_missing and os.path.exists(os.path.join(out, "manifest.txt")):
        print("[pack] assets ja existem em", out, "- nada a fazer")
        return 0

    cfg = read_cfg(os.path.join(args.config, "stage1.cfg"))
    limit = args.max_candidates or cfg["max"] or 40

    images = []
    if args.dump and not args.placeholder and os.path.isdir(args.dump):
        images = scan_images(args.dump)
    print("[pack] imagens encontradas:", len(images))

    placeholder = not images
    window = cfg["window"] or (detect_window(images) if images else (1280, 720))
    if window[0] > 2048 or window[1] > 1152:  # o leitor trabalha com imagens de ate 2048x1152
        k = min(2048.0 / window[0], 1152.0 / window[1])
        window = (int(window[0] * k), int(window[1] * k))
    print("[pack] janela do jogo:", window)

    # limpa a saida anterior
    for sub in ("img", "frames"):
        shutil.rmtree(os.path.join(out, sub), ignore_errors=True)
        os.makedirs(os.path.join(out, sub), exist_ok=True)

    entries = []  # (arquivo_fwi, w, h, nome_original, caminho_origem)
    seen_hash = set()
    if placeholder:
        im = make_placeholder_image(*window)
        data = fwi_encode(im)
        with open(os.path.join(out, "img", "0000.fwi"), "wb") as f:
            f.write(data)
        entries.append(("img/0000.fwi", im.width, im.height, "placeholder", None))
    else:
        cands = pick_candidates(images, window, cfg["prefer"], limit * 3)
        for d in cands:
            if len(entries) >= limit:
                break
            try:
                with Image.open(d["path"]) as im:
                    im.load()
                    im = im.convert("RGBA")
            except Exception as e:
                print("[pack] pulei (erro ao abrir):", d["rel"], e)
                continue
            max_w, max_h = min(window[0], 2048), min(window[1], 1152)
            if im.width > max_w or im.height > max_h:
                im.thumbnail((max_w, max_h), Image.LANCZOS)  # reduz mantendo a proporcao
            hsh = hashlib.md5(im.tobytes()).hexdigest()
            if hsh in seen_hash:
                continue  # imagem duplicada
            seen_hash.add(hsh)
            idx = len(entries)
            name = "img/%04d.fwi" % idx
            data = fwi_encode(im)
            with open(os.path.join(out, name), "wb") as f:
                f.write(data)
            entries.append((name, im.width, im.height, d["rel"], d["path"]))
    if not entries:
        im = make_placeholder_image(*window)
        with open(os.path.join(out, "img", "0000.fwi"), "wb") as f:
            f.write(fwi_encode(im))
        entries.append(("img/0000.fwi", im.width, im.height, "placeholder", None))
        placeholder = True

    # manifest.txt
    lines = ["fwmanifest 1", "window %d %d" % window, "placeholder %d" % (1 if placeholder else 0)]
    for i, (name, w, h, orig, _src) in enumerate(entries):
        short = re.sub(r"\s+", "_", orig)[-90:]
        lines.append("image %d %s %d %d %s" % (i, name, w, h, short))
    lines += ["title 0", "frame title frames/title.frame"]
    with open(os.path.join(out, "manifest.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    # frames/title.frame
    hot = read_hotspots(os.path.join(args.config, "hotspots_title.txt"))
    frame = ["# Tela inicial. Camada 'title' = imagem selecionada no leitor.", "frame title %d %d" % window, "layer title 0 0"] + hot
    with open(os.path.join(out, "frames", "title.frame"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(frame) + "\n")

    total = sum(os.path.getsize(os.path.join(out, e[0])) for e in entries)
    print("[pack] %d candidatas gravadas (%.1f MB) em %s" % (len(entries), total / 1048576.0, out))

    # relatorio
    if args.report:
        os.makedirs(args.report, exist_ok=True)
        with open(os.path.join(args.report, "imagens.txt"), "w", encoding="utf-8") as f:
            f.write("# %d imagens encontradas (arquivo, largura x altura)\n" % len(images))
            for d in images[:5000]:
                f.write("%s\t%dx%d\n" % (d["rel"], d["w"], d["h"]))
        if not placeholder:
            contact_sheets([("#%d  %dx%d" % (i, e[1], e[2]), e[4]) for i, e in enumerate(entries)],
                           args.report, "contato_candidatas")
            big = sorted(images, key=lambda d: -d["w"] * d["h"])[:100]
            contact_sheets([("%dx%d" % (d["w"], d["h"]), d["path"]) for d in big], args.report, "contato_maiores", per_sheet=20)
        sizes = Counter((d["w"], d["h"]) for d in images)
        summary = {
            "imagens_encontradas": len(images),
            "tamanhos_mais_comuns": [{"tamanho": "%dx%d" % k, "qtd": v} for k, v in sizes.most_common(10)],
            "maiores": ["%dx%d" % (d["w"], d["h"]) for d in sorted(images, key=lambda d: -d["w"] * d["h"])[:5]],
            "janela": window,
            "placeholder": placeholder,
            "candidatas": [{"indice": i, "fwi": e[0], "w": e[1], "h": e[2], "origem": e[3]} for i, e in enumerate(entries)],
            "tamanho_total_mb": round(total / 1048576.0, 2),
        }
        with open(os.path.join(args.report, "pack_report.txt"), "w", encoding="utf-8") as f:
            f.write(json.dumps(summary, indent=2, ensure_ascii=False))

    if args.dump and not args.placeholder and placeholder:
        if images:
            print("[pack] ERRO: %d imagens foram extraidas, mas nenhuma serviu como candidata (veja 'tamanhos_mais_comuns' no relatorio)." % len(images))
        else:
            print("[pack] ERRO: NENHUMA imagem foi extraida do .exe. Foi gerada so a imagem de teste (placeholder).")
        print("[pack]       Veja o resumo no topo da pagina do run e o artefato 'relatorio-extracao' (extract_log.txt).")
        return 3  # deixa o workflow VERMELHO para voce perceber que a extracao nao funcionou
    return 0


if __name__ == "__main__":
    sys.exit(main())
