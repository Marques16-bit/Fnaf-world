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
import json
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


def gh(args):
    try:
        return subprocess.run(["gh"] + args, capture_output=True, text=True)
    except FileNotFoundError:
        return None


def list_releases(repo):
    """Lista as releases PUBLICADAS que o token enxerga (rascunhos/drafts nao aparecem)."""
    r = gh(["api", "repos/%s/releases?per_page=30" % repo])
    if r is None:
        log("gh CLI nao encontrado")
        return None
    if r.returncode != 0:
        log("nao consegui listar as releases:", (r.stderr or r.stdout).strip()[:300])
        return None
    try:
        return json.loads(r.stdout)
    except Exception as e:
        log("resposta inesperada ao listar releases:", repr(e))
        return None


def pick_asset(assets):
    """Escolhe o melhor anexo: .exe (de preferencia com fnaf/world no nome), senao .zip."""
    def score(a):
        n = a["name"].lower()
        s = a.get("size", 0)
        if "fnaf" in n or "world" in n:
            s += 10 ** 12
        return s
    for ext in (".exe", ".zip"):
        c = [a for a in assets if a["name"].lower().endswith(ext)]
        if c:
            return sorted(c, key=score, reverse=True)[0]
    return None


def from_releases(repo, tag, out_dir):
    rels = list_releases(repo)
    if rels is None:
        return None
    if not rels:
        log("Este repositorio NAO tem nenhuma release PUBLICADA. (Rascunho/'draft' nao conta: clique em 'Publish release'.)")
        return None
    log("Releases visiveis neste repositorio:")
    for r in rels:
        names = ", ".join("%s (%.1f MB)" % (a["name"], a.get("size", 0) / 1048576.0) for a in r.get("assets", [])) or "(sem anexos)"
        log("  - tag '%s'%s: %s" % (r.get("tag_name"), " [pre-release]" if r.get("prerelease") else "", names))
    ordered = [r for r in rels if r.get("tag_name") == tag] + [r for r in rels if r.get("tag_name") != tag]
    if not any(r.get("tag_name") == tag for r in rels):
        log("AVISO: nao existe release publicada com a tag '%s'; vou procurar um .exe/.zip nas outras." % tag)
    rel_dir = os.path.join(out_dir, "release")
    shutil.rmtree(rel_dir, ignore_errors=True)
    os.makedirs(rel_dir)
    for r in ordered:
        asset = pick_asset(r.get("assets", []))
        if not asset:
            continue
        log("baixando '%s' da release '%s' ..." % (asset["name"], r.get("tag_name")))
        res = gh(["release", "download", r["tag_name"], "-R", repo, "-D", rel_dir, "--pattern", asset["name"], "--clobber"])
        if res is None or res.returncode != 0:
            log("falhou:", ((res.stderr or res.stdout) if res else "gh ausente").strip()[:300])
            continue
        path = os.path.join(rel_dir, asset["name"])
        if not os.path.exists(path):
            found = glob.glob(os.path.join(rel_dir, "*"))
            path = found[0] if found else None
        if not path:
            continue
        exe = unzip_if_needed(path, out_dir) if path.lower().endswith(".zip") else path
        if exe:
            log("achei:", exe)
            return exe
    log("Nenhuma release tem um .exe ou .zip anexado (so esses formatos sao aceitos).")
    return None


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
        exe = from_releases(args.repo, args.tag, args.out)

    if not exe or not os.path.exists(exe):
        log("NAO ENCONTREI o FNaFWorld.exe.")
        log("Como resolver: no repositorio va em Releases > 'Draft a new release', escolha/crie a tag '%s', "
            "anexe o FNaFWorld.exe (ou um .zip do jogo) e clique em 'Publish release' (NAO deixe como rascunho)." % args.tag)
        log("Alternativa: rode o workflow de novo e cole um link DIRETO de download no campo exe_url.")
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
