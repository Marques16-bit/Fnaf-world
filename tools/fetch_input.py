#!/usr/bin/env python3
"""fetch_input.py - encontra/baixa o FNaFWorld.exe e copia para work/input/.

Ordem de busca:
  1. --url (entrada do workflow) -> baixa o arquivo (.exe ou .zip com o jogo)
  2. input/*.exe dentro do proprio repositorio (limite do GitHub: 100 MB; pelo navegador do celular: 25 MB)
  3. Release do proprio repositorio (tag --tag, padrao "assets") com um .exe ou .zip anexado
     (Releases aceitam arquivos de ate 2 GB mesmo pelo navegador do celular. Repositorio PRIVADO = so voce ve.)

Saida: imprime o caminho do .exe e, se existir $GITHUB_OUTPUT, grava exe_path=...
Codigo de saida 1 se nao achar nada.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile


def log(*a):
    print("[fetch]", *a, flush=True)


def pick_exe(folder):
    exes = []
    for root, _d, files in os.walk(folder):
        for f in files:
            if f.lower().endswith(".exe"):
                p = os.path.join(root, f)
                score = 0
                low = f.lower()
                if "fnaf" in low or "world" in low:
                    score += 1000000000000
                score += os.path.getsize(p)
                exes.append((score, p))
    if not exes:
        return None
    exes.sort(reverse=True)
    return exes[0][1]


def unzip_if_needed(path, workdir):
    with open(path, "rb") as f:
        magic = f.read(4)
    if magic[:2] == b"PK":
        dest = os.path.join(workdir, "unzipped")
        shutil.rmtree(dest, ignore_errors=True)
        os.makedirs(dest)
        log("descompactando", path)
        with zipfile.ZipFile(path) as z:
            z.extractall(dest)
        return pick_exe(dest)
    return path


def download(url, dest):
    log("baixando", url)
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (fnafworld-ps4 ci)"})
    with urllib.request.urlopen(req, timeout=600) as r, open(dest, "wb") as out:
        shutil.copyfileobj(r, out, 1024 * 1024)
    size = os.path.getsize(dest)
    log("baixado: %.1f MB" % (size / 1048576.0))
    with open(dest, "rb") as f:
        head = f.read(512).lower()
    if head.startswith(b"<!doctype html") or head.startswith(b"<html"):
        raise RuntimeError("o link devolveu uma pagina HTML, nao o arquivo. Use um link DIRETO de download "
                           "(Google Drive/Mega normalmente nao funcionam; prefira a opcao Release do GitHub).")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="work/input")
    ap.add_argument("--url", default="")
    ap.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", ""))
    ap.add_argument("--tag", default="assets")
    ap.add_argument("--repo-input", default="input")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    exe = None

    if args.url.strip():
        try:
            dest = os.path.join(args.out, "download.bin")
            download(args.url.strip(), dest)
            exe = unzip_if_needed(dest, args.out)
        except Exception as e:
            log("ERRO no download:", e)

    if not exe and os.path.isdir(args.repo_input):
        exe = pick_exe(args.repo_input)
        if exe:
            log("achei no repositorio:", exe)

    if not exe and args.repo:
        rel_dir = os.path.join(args.out, "release")
        shutil.rmtree(rel_dir, ignore_errors=True)
        os.makedirs(rel_dir)
        for pattern in ("*.exe", "*.zip"):
            cmd = ["gh", "release", "download", args.tag, "-R", args.repo, "-D", rel_dir, "--pattern", pattern, "--clobber"]
            log("rodando:", " ".join(cmd))
            try:
                r = subprocess.run(cmd, capture_output=True, text=True)
                if r.returncode != 0:
                    log("gh (", pattern, ") ->", (r.stderr or r.stdout).strip()[:300])
            except FileNotFoundError:
                log("gh CLI nao encontrado")
                break
            files = glob.glob(os.path.join(rel_dir, pattern))
            if files:
                files.sort(key=os.path.getsize, reverse=True)
                exe = unzip_if_needed(files[0], args.out) if pattern == "*.zip" else files[0]
                if exe:
                    log("achei na release:", exe)
                    break

    if not exe or not os.path.exists(exe):
        log("NAO ENCONTREI o FNaFWorld.exe. Veja o README: coloque-o em uma Release com a tag '%s' "
            "(Releases > Draft a new release) ou informe um link direto ao rodar o workflow." % args.tag)
        return 1

    final = os.path.join(args.out, "game.exe")
    if os.path.abspath(exe) != os.path.abspath(final):
        shutil.copyfile(exe, final)
    log("EXE pronto: %s (%.1f MB)" % (final, os.path.getsize(final) / 1048576.0))
    out_file = os.environ.get("GITHUB_OUTPUT")
    if out_file:
        with open(out_file, "a") as f:
            f.write("exe_path=%s\n" % final.replace("\\", "/"))
    print(final)
    return 0


if __name__ == "__main__":
    sys.exit(main())
