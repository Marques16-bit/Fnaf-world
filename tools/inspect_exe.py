#!/usr/bin/env python3
"""inspect_exe.py - diagnostico rapido de um .exe feito no Clickteam Fusion 2.5.

Nao extrai nada: so le o cabecalho e a lista de "chunks" para descobrir a VERSAO/BUILD do runtime
(importante para escolher o extrator certo). O resultado vai para o log do GitHub Actions.

Uso: python tools/inspect_exe.py caminho/FNaFWorld.exe [saida.txt]
"""
import mmap
import struct
import sys
from collections import OrderedDict

CHUNK_NAMES = {
    0x2223: "AppHeader", 0x2224: "AppName", 0x2225: "Author", 0x2226: "Menu", 0x2227: "ExtPath",
    0x222E: "EditorFilename", 0x222F: "TargetFilename", 0x2230: "AppDoc", 0x2231: "OtherExts",
    0x2232: "GlobalValues", 0x2233: "GlobalStrings", 0x2234: "Extensions", 0x2235: "AppIcon",
    0x2236: "DemoVersion", 0x2237: "SecNum", 0x2238: "BinaryFiles", 0x2239: "AppMenuImages",
    0x223A: "AboutText", 0x223B: "Copyright", 0x223C: "GlobalValueNames", 0x223D: "GlobalStringNames",
    0x223E: "MovementExts", 0x223F: "ObjectInfos(FrameItems)", 0x2240: "FrameHandles",
    0x2245: "ExtendedHeader", 0x2246: "SpacerChunk", 0x224D: "FrameHandles2", 0x224F: "ExeOnly",
    0x3333: "Frame", 0x3334: "FrameHeader", 0x3335: "FrameName", 0x3336: "FramePassword",
    0x3337: "FramePalette", 0x3338: "FrameObjectInstances", 0x333B: "FrameFadeIn", 0x333D: "FrameEvents",
    0x3341: "FrameLayers", 0x3342: "FrameVirtualRect", 0x3345: "FrameEffects",
    0x5555: "ImageOffsets", 0x6666: "ImageBank", 0x6667: "SoundOffsets", 0x6668: "SoundBank",
    0x6669: "MusicOffsets", 0x666A: "MusicBank", 0x7F7F: "Last",
}


def pe_end(m):
    if m[:2] != b"MZ":
        return None
    e = struct.unpack_from("<I", m, 0x3C)[0]
    if m[e:e + 4] != b"PE\0\0":
        return None
    nsec = struct.unpack_from("<H", m, e + 6)[0]
    opt = struct.unpack_from("<H", m, e + 20)[0]
    tbl = e + 24 + opt
    end = 0
    for i in range(nsec):
        raw_size, raw_ptr = struct.unpack_from("<II", m, tbl + i * 40 + 16)
        end = max(end, raw_ptr + raw_size)
    return end


def find_game_header(m, start):
    for sig in (b"PAMU", b"PAME"):
        pos = start
        while True:
            pos = m.find(sig, pos)
            if pos < 0 or pos + 24 > len(m):
                break
            rv, rsv, pv, pb = struct.unpack_from("<HHII", m, pos + 4)
            cid, cflags, csize = struct.unpack_from("<HHI", m, pos + 16)
            if 0x1000 <= cid <= 0x7FFF and csize < len(m) and rv in (0x0200, 0x0300, 0x0301, 0x0302, 0x0303):
                return pos, sig, rv, rsv, pv, pb
            pos += 4
    return None


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = sys.argv[1]
    lines = []

    def out(s=""):
        print(s)
        lines.append(s)

    with open(path, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        out("Arquivo: %s (%.1f MB)" % (path, len(m) / 1048576.0))
        end = pe_end(m)
        out("Fim das secoes do PE (inicio dos dados anexados): %s" % (hex(end) if end is not None else "nao e PE valido"))
        hdr = find_game_header(m, end or 0) or find_game_header(m, 0)
        if not hdr:
            out("NAO achei o cabecalho PAME/PAMU do Clickteam. O .exe pode estar empacotado/protegido ou os dados estar em arquivo externo (.dat/.ccn).")
        else:
            pos, sig, rv, rsv, pv, pb = hdr
            out("Cabecalho: %s em 0x%x  (%s)" % (sig.decode(), pos, "Unicode" if sig == b"PAMU" else "ANSI"))
            out("Runtime version: 0x%04x  subversion: 0x%04x  product version: 0x%x  BUILD: %d" % (rv, rsv, pv, pb))
            p = pos + 16
            stats = OrderedDict()
            n = 0
            while p + 8 <= len(m) and n < 200000:
                cid, cflags, csize = struct.unpack_from("<HHI", m, p)
                s = stats.setdefault(cid, [0, 0, cflags])
                s[0] += 1
                s[1] += csize
                p += 8 + csize
                n += 1
                if cid == 0x7F7F:
                    break
            out("Chunks de nivel superior: %d  (fim em 0x%x)" % (n, p))
            out("%-8s %-26s %6s %12s %s" % ("id", "nome (aproximado)", "qtd", "bytes", "flags(1o)"))
            for cid, (cnt, size, fl) in stats.items():
                out("0x%04X   %-26s %6d %12d %d" % (cid, CHUNK_NAMES.get(cid, "?"), cnt, size, fl))
    if len(sys.argv) > 2:
        with open(sys.argv[2], "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:  # diagnostico nunca deve derrubar o workflow
        print("inspect_exe falhou (ignorado):", repr(e))
        sys.exit(0)
