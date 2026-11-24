#!/usr/bin/env python3
"""extract_assets.py - orquestra o extrator Clickteam (CTFAK 2.0) para tirar as imagens do FNaFWorld.exe.

Roda no runner WINDOWS do GitHub Actions (o CTFAK 2.0 e um programa .NET 6 para Windows).
Este script (Python 3) faz tudo que um humano faria a mao:

  1. acha o CTFAK.Cli.exe ja compilado (workflow faz o 'dotnet build')
  2. copia os plugins (Dumper.dll, CTFAK.Decompiler.dll) para a pasta Plugins ao lado do CLI
  3. baixa o ctfakrequirements.zip oficial e extrai ao lado do CLI
  4. roda o CLI em modo automatico:   CTFAK.Cli.exe -path "jogo.exe" -tool "Image Dumper" -closeonfinish
     (se uma tentativa nao gerar imagens, tenta a proxima: outros parametros / outras ferramentas)
  5. junta todas as imagens geradas em --dump-out e grava um relatorio

Nada aqui falha o workflow de forma "dura": se der erro, o relatorio explica e o resto do pipeline
continua com assets de teste (placeholder), para voce sempre ter um PKG para testar.

Uso:
    python tools/extract_assets.py --exe work/input/game.exe --ctfak-src _ctfak --work work --dump-out work/dump
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

REQUIREMENTS_URL = "https://github.com/CTFAK/.github/raw/main/ctfakrequirements.zip"
IMG_EXT = (".png", ".bmp", ".jpg", ".jpeg", ".tga")

# (ferramenta, parametros extras)  -> tentadas em ordem ate sair um bom numero de imagens
ATTEMPTS = [
    ("Image Dumper", ""),
    ("Image Dumper", "-onlyimages"),
    ("Dump Everything", "-nosounds"),
    ("Sorted Image Dumper", ""),
]
MIN_IMAGES = 10

_LOG = []


def log(*a):
    msg = " ".join(str(x) for x in a)
    print("[extract]", msg, flush=True)
    _LOG.append(msg)


def find_files(root, names, must_contain=None):
    names_l = {n.lower() for n in names}
    out = []
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f.lower() in names_l:
                p = os.path.join(dp, f)
                if must_contain is None or must_contain.lower() in p.replace("\\", "/").lower():
                    out.append(p)
    return out


def newest(paths):
    return sorted(paths, key=os.path.getmtime, reverse=True)


def find_cli(src):
    cands = find_files(src, ["CTFAK.Cli.exe"], "/bin/")
    if not cands:
        cands = find_files(src, ["CTFAK.Cli.exe"])
    rel = [p for p in cands if "release" in p.replace("\\", "/").lower()]
    pick = newest(rel or cands)
    return pick[0] if pick else None


def stage_plugins(src, cli_dir):
    """Copia <Projeto>.dll de cada Plugins/<Projeto>/bin/**/ para <cli_dir>/Plugins/."""
    plugins_src = os.path.join(src, "Plugins")
    dest = os.path.join(cli_dir, "Plugins")
    os.makedirs(dest, exist_ok=True)
    copied = []
    if not os.path.isdir(plugins_src):
        log("AVISO: pasta Plugins nao encontrada em", src)
        return copied
    for proj in sorted(os.listdir(plugins_src)):
        pdir = os.path.join(plugins_src, proj)
        if not os.path.isdir(pdir):
            continue
        dlls = find_files(pdir, [proj + ".dll"], "/bin/")
        if not dlls:
            log("  plugin sem DLL compilada:", proj)
            continue
        dll = newest(dlls)[0]
        shutil.copy2(dll, dest)
        pdb = dll[:-4] + ".pdb"
        if os.path.exists(pdb):
            shutil.copy2(pdb, dest)
        copied.append(os.path.basename(dll))
    log("plugins copiados:", ", ".join(copied) if copied else "(nenhum)")
    return copied


def fetch_requirements(cli_dir):
    if os.path.exists(os.path.join(cli_dir, "template.mfa")):
        log("ctfakrequirements ja presente")
        return True
    try:
        zpath = os.path.join(cli_dir, "ctfakrequirements.zip")
        log("baixando", REQUIREMENTS_URL)
        req = urllib.request.Request(REQUIREMENTS_URL, headers={"User-Agent": "fnafworld-ps4-ci"})
        with urllib.request.urlopen(req, timeout=120) as r, open(zpath, "wb") as f:
            shutil.copyfileobj(r, f)
        with zipfile.ZipFile(zpath) as z:
            z.extractall(cli_dir)
        log("ctfakrequirements extraido; template.mfa presente:", os.path.exists(os.path.join(cli_dir, "template.mfa")))
        return True
    except Exception as e:
        log("ERRO ao baixar/extrair ctfakrequirements:", repr(e))
        return False


def list_images(root):
    out = []
    if not os.path.isdir(root):
        return out
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f.lower().endswith(IMG_EXT):
                out.append(os.path.join(dp, f))
    return out


def snapshot_images(roots):
    s = set()
    for r in roots:
        s.update(list_images(r))
    return s


def run_cli(cli, exe, tool, params, cwd, timeout_s):
    cmd = [cli, "-path", exe, "-tool", tool, "-closeonfinish"]
    if params:
        cmd[3:3] = ["-parameters", params]
    log("executando:", " ".join('"%s"' % c if " " in c else c for c in cmd))
    t0 = time.time()
    try:
        p = subprocess.Popen(cmd, cwd=cwd, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True, errors="replace")
        try:
            out, _ = p.communicate(timeout=timeout_s)
        except subprocess.TimeoutExpired:
            p.kill()
            out, _ = p.communicate()
            log("TEMPO ESGOTADO (%d s); processo encerrado" % timeout_s)
        code = p.returncode
    except Exception as e:
        log("ERRO ao executar o CLI:", repr(e))
        return -1, ""
    log("codigo de saida: %s  (%.0f s)" % (code, time.time() - t0))
    tail = "\n".join((out or "").splitlines()[-40:])
    log("---- ultimas linhas do CTFAK ----\n" + tail + "\n---------------------------------")
    return code, out or ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--ctfak-src", default="_ctfak")
    ap.add_argument("--work", default="work")
    ap.add_argument("--dump-out", default="work/dump")
    ap.add_argument("--timeout-min", type=int, default=30)
    args = ap.parse_args()

    os.makedirs(args.work, exist_ok=True)
    report = {"ok": False, "images": 0, "attempts": [], "cli": None}

    def finish(code):
        with open(os.path.join(args.work, "extract_log.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(_LOG) + "\n")
        with open(os.path.join(args.work, "extract_report.json"), "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        return code

    if not os.path.exists(args.exe):
        log("ERRO: exe nao encontrado:", args.exe)
        return finish(1)

    cli = find_cli(args.ctfak_src)
    if not cli:
        log("ERRO: CTFAK.Cli.exe nao foi encontrado em", args.ctfak_src, "- o 'dotnet build' provavelmente falhou.")
        return finish(1)
    cli_dir = os.path.dirname(os.path.abspath(cli))
    report["cli"] = cli
    log("CLI:", cli)

    stage_plugins(args.ctfak_src, cli_dir)
    fetch_requirements(cli_dir)

    exe_abs = os.path.abspath(args.exe)
    timeout_s = args.timeout_min * 60
    watch = [os.path.join(cli_dir, "Dumps"), os.path.join(os.getcwd(), "Dumps"), os.path.join(args.work, "Dumps")]
    best = []

    for tool, params in ATTEMPTS:
        before = snapshot_images(watch)
        code, _out = run_cli(os.path.abspath(cli), exe_abs, tool, params, cli_dir, timeout_s)
        after = snapshot_images(watch)
        new = sorted(after - before) or sorted(after)
        report["attempts"].append({"tool": tool, "params": params, "exit": code, "images": len(new)})
        log("tentativa '%s' %s -> %d imagens" % (tool, params, len(new)))
        if len(new) > len(best):
            best = new
        if len(new) >= MIN_IMAGES:
            break

    # copia as imagens para --dump-out preservando a estrutura relativa
    shutil.rmtree(args.dump_out, ignore_errors=True)
    os.makedirs(args.dump_out, exist_ok=True)
    for p in best:
        root = next((w for w in watch if p.startswith(w)), os.path.dirname(p))
        rel = os.path.relpath(p, root)
        dst = os.path.join(args.dump_out, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(p, dst)
    report["images"] = len(best)
    report["ok"] = len(best) >= MIN_IMAGES
    log("TOTAL de imagens coletadas:", len(best), "-> OK" if report["ok"] else "-> POUCO/NENHUM (veja o log acima)")
    return finish(0 if report["ok"] else 1)


if __name__ == "__main__":
    sys.exit(main())
