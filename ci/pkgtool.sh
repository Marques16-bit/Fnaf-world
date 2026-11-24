#!/usr/bin/env bash
# Roda o PkgTool.Core dentro de um container Ubuntu 20.04 (que tem libssl1.1).
# Usado pelo workflow 2 SOMENTE se o PkgTool.Core nao rodar direto no runner.
exec docker run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp -e TMPDIR=/tmp -e DOTNET_BUNDLE_EXTRACT_BASE_DIR=/tmp/.net \
  -e DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 \
  -v "$PWD":"$PWD" -v "$OO_PS4_TOOLCHAIN":"$OO_PS4_TOOLCHAIN":ro \
  -w "$PWD" pkgtool-env "$OO_PS4_TOOLCHAIN/bin/linux/PkgTool.Core" "$@"
