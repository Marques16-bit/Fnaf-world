#!/usr/bin/env python3
"""fix_gp4.py - conserta o pkg.gp4 gerado pelo create-gp4 do OpenOrbis.

O create-gp4 so declara em <rootdir> um conjunto FIXO de pastas (sce_sys, sce_sys/about, sce_module...).
Se algum arquivo estiver em outra pasta (ex.: assets/img/0000.fwi), o PkgTool.Core quebra com
"System.InvalidOperationException: Sequence contains no elements ... PfsProperties.FindDir".

Este script le todos os <file targ_path="..."> do gp4 e reescreve <rootdir> com TODAS as pastas necessarias.

Uso: python3 ci/fix_gp4.py pkg.gp4
"""
import sys
import xml.etree.ElementTree as ET


def main(path):
    tree = ET.parse(path)
    root = tree.getroot()
    files = root.find("files")
    if files is None:
        print("fix_gp4: ERRO: o gp4 nao tem <files>")
        return 1

    dirs = set()
    nfiles = 0
    for f in files.iter("file"):
        nfiles += 1
        tp = (f.get("targ_path") or "").replace("\\", "/").strip("/")
        parts = tp.split("/")[:-1]
        for i in range(1, len(parts) + 1):
            dirs.add("/".join(parts[:i]))

    rd = root.find("rootdir")
    if rd is None:
        rd = ET.SubElement(root, "rootdir")
    for child in list(rd):
        rd.remove(child)

    nodes = {"": rd}
    for d in sorted(dirs, key=lambda s: (s.count("/"), s)):
        parent, _, name = d.rpartition("/")
        nodes[d] = ET.SubElement(nodes[parent], "dir", {"targ_name": name})

    ET.indent(tree, space="  ")
    tree.write(path, encoding="utf-8", xml_declaration=True)
    print("fix_gp4: %d arquivos, %d pastas declaradas: %s" % (nfiles, len(dirs), ", ".join(sorted(dirs)) or "(nenhuma)"))
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
