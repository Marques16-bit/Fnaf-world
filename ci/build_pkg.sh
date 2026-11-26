#!/usr/bin/env bash
# Gera o .pkg: tenta o PkgTool.Core direto; se falhar (ex.: "No usable version of libssl was found"),
# repete dentro de um container Ubuntu 20.04, que tem a libssl 1.1.
# Uso: build_pkg.sh pkg.gp4 .      (rodar dentro da pasta ps4/; precisa de OO_PS4_TOOLCHAIN)
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PK="${PKGTOOL:-$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core}"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1

echo ">> Conteudo que vai dentro do PKG:"
echo "   assets: $(find assets -type f ! -name .gitkeep 2>/dev/null | wc -l) arquivos, $(du -sh assets 2>/dev/null | cut -f1)"
head -n 4 assets/manifest.txt 2>/dev/null | sed 's/^/   /'
grep -c '^image ' assets/manifest.txt 2>/dev/null | sed 's/^/   imagens no manifesto: /'
if grep -q '^placeholder 1' assets/manifest.txt 2>/dev/null; then
  echo "   !!! ATENCAO: o PKG vai SO com a imagem de teste (nao ha assets extraidos) !!!"
fi

if "$PK" pkg_build "$@"; then
  exit 0
fi

echo ">> pkg_build direto falhou (provavelmente falta a libssl 1.1). Tentando em container Ubuntu 20.04..."
if ! command -v docker >/dev/null 2>&1; then
  echo ">> ERRO: docker nao encontrado neste ambiente."
  exit 1
fi
docker build -t pkgtool-env -f "$HERE/Dockerfile.pkgtool" "$HERE" || exit 1
bash "$HERE/pkgtool.sh" pkg_build "$@"
