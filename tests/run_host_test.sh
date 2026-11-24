#!/usr/bin/env bash
# Testa o nucleo C++ no PC/Linux (sem PS4): gera assets de teste, compila com sanitizers e renderiza um quadro.
# Uso: bash tests/run_host_test.sh      (precisa de g++, python3, pillow, numpy)
set -euo pipefail
cd "$(dirname "$0")/.."
T=$(mktemp -d)
python3 - "$T" <<'PY'
import sys, os
from PIL import Image, ImageDraw
t = sys.argv[1]
os.makedirs(t + "/dump", exist_ok=True)
for i in range(4):
    im = Image.new("RGBA", (1280, 720), (20 + i * 40, 40, 90, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([100, 100, 1180, 620], outline=(255, 255, 255, 255), width=8)
    d.ellipse([400, 200, 880, 520], fill=(200, 60 + i * 30, 60, 255))
    im.save("%s/dump/%d.png" % (t, 1000 + i))
PY
python3 tools/pack_assets.py --dump "$T/dump" --out "$T/assets" --config config
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter \
  ps4/src/core/*.cpp ps4/src/platform/host/platform_host.cpp tests/host_main.cpp -o "$T/host_test"
"$T/host_test" "$T/assets" "$T/out.ppm" 14 "10:cross,12:triangle"
python3 -c "from PIL import Image; Image.open('$T/out.ppm').save('$T/out.png')"
echo "OK -> $T/out.png"
