#!/usr/bin/env python3
"""Gera um resumo em Markdown para a pagina do run (GITHUB_STEP_SUMMARY), legivel no celular."""
import json
import os
import re


def read(path, limit=4000):
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8", errors="replace") as f:
        t = f.read()
    return t if len(t) <= limit else t[-limit:]


def jload(path):
    t = read(path, 10 ** 7)
    if not t:
        return None
    try:
        return json.loads(t)
    except Exception:
        return None


def main():
    rep = jload("work/extract_report.json")
    pk = jload("work/relatorio/pack_report.txt")

    # ---------- veredito ----------
    n_img = (rep or {}).get("images", 0)
    placeholder = (pk or {}).get("placeholder", True)
    ncand = len((pk or {}).get("candidatas", []))
    if pk and not placeholder and ncand > 0:
        print("## ✅ Extracao OK: %d imagens extraidas, %d candidatas vao para o PKG (%.1f MB)\n" % (
            (pk or {}).get("imagens_encontradas", n_img), ncand, (pk or {}).get("tamanho_total_mb", 0)))
    else:
        print("## ❌ A extracao NAO funcionou: o PKG sairia so com a imagem de teste\n")

    # ---------- causa provavel ----------
    build = read("work/dotnet_build.log", 200000) or ""
    if rep is None:
        print("- O extrator nem chegou a rodar (falhou antes: veja o passo vermelho acima).")
    elif rep.get("erro"):
        print("- **Causa:** %s." % rep["erro"])
        errs = [l.strip() for l in build.splitlines() if re.search(r"error\s+\w+\d+:|error CS|Build FAILED|erro", l, re.I)]
        if errs:
            print("\nPrimeiros erros da compilacao do CTFAK:\n```")
            print("\n".join(errs[:12]))
            print("```")
    else:
        print("- Imagens extraidas pelo CTFAK: **%s**" % n_img)
        for a in rep.get("attempts", []):
            print("  - `%s %s` -> %s imagens (codigo %s, %ss, %s arquivos novos)" % (
                a["tool"], a["params"], a["images"], a["exit"], a.get("seconds", "?"), a.get("new_files", "?")))
    if pk:
        print("- Janela do jogo: **%sx%s**" % tuple(pk["janela"]))
        if pk.get("tamanhos_mais_comuns"):
            print("- Tamanhos de imagem mais comuns: " + ", ".join("%s (x%d)" % (t["tamanho"], t["qtd"]) for t in pk["tamanhos_mais_comuns"][:6]))

    insp = read("work/inspect.txt", 2500)
    if insp:
        print("\n### Versao do Clickteam no .exe\n```\n%s\n```" % insp)
    log = read("work/extract_log.txt", 3500)
    if log:
        print("\n### Final do log do extrator\n```\n%s\n```" % log)
    print("\nArtefato **relatorio-extracao**: log completo, saida de cada tentativa do CTFAK (`ctfak_tentativa_N.txt`) e miniaturas.")


if __name__ == "__main__":
    main()
