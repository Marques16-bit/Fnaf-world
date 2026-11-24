#!/usr/bin/env python3
"""Gera um resumo em Markdown para a pagina do run (GITHUB_STEP_SUMMARY), legivel no celular."""
import json
import os


def read(path, limit=4000):
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8", errors="replace") as f:
        t = f.read()
    return t if len(t) <= limit else t[:limit] + "\n... (cortado; veja o artefato 'relatorio-extracao')"


def main():
    print("## Extracao de assets do FNaF World\n")
    rep = read("work/extract_report.json")
    if rep:
        try:
            r = json.loads(rep)
            print("- Imagens extraidas: **%s**  (%s)" % (r.get("images"), "OK" if r.get("ok") else "FALHOU/POUCAS"))
            for a in r.get("attempts", []):
                print("  - tentativa `%s %s` -> %s imagens (codigo %s)" % (a["tool"], a["params"], a["images"], a["exit"]))
        except Exception:
            print(rep)
    else:
        print("- A extracao nao chegou a rodar (veja o passo que falhou acima).")

    pk = read("work/relatorio/pack_report.txt", 3000)
    if pk:
        try:
            p = json.loads(pk)
            print("- Janela do jogo detectada: **%sx%s**" % tuple(p["janela"]))
            print("- Candidatas de titulo no pacote do PS4: **%d**  (%.1f MB)%s" % (
                len(p["candidatas"]), p["tamanho_total_mb"], "  ⚠️ MODO DE TESTE (sem assets reais)" if p["placeholder"] else ""))
        except Exception:
            pass

    insp = read("work/inspect.txt", 2500)
    if insp:
        print("\n### Versao do Clickteam no .exe\n```\n%s\n```" % insp)
    log = read("work/extract_log.txt", 3500)
    if log:
        print("\n### Log do extrator\n```\n%s\n```" % log[-3500:])
    print("\nAs miniaturas (folhas de contato) estao no artefato **relatorio-extracao**.")


if __name__ == "__main__":
    main()
