#!/bin/sh
# Builds the launcher (this repo) plus both Translation Core pieces (the
# sibling AHNX-Translation-Core repo) and arranges them into testingbuild/
# matching the real SD card layout — drag the contents straight onto your
# SD card:
#   testingbuild/AndroidHorizonNX.nro                              (the launcher)
#   testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x64.nro    (real engine)
#   testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x32.nro    (placeholder)
#
# Expects https://github.com/AndroidHorizon/AHNX-Translation-Core to be
# cloned as a SIBLING of this repo (../AHNX-Translation-Core) — that's the
# only thing this script assumes about your layout:
#   git clone https://github.com/AndroidHorizon/AndroidHorizonNX.git
#   git clone https://github.com/AndroidHorizon/AHNX-Translation-Core.git
#   cd AndroidHorizonNX && ./build_all.sh
#
# Copy testingbuild/AndroidHorizonNX.nro to sdmc:/switch/ and
# testingbuild/AndroidHorizonNX/ to sdmc:/switch/AndroidHorizonNX/ to match
# what the launcher expects (see CORE_X64_PATH/CORE_X32_PATH in
# source/main.cpp).
set -e
cd "$(dirname "$0")"

CORE_DIR="../AHNX-Translation-Core"
if [ ! -d "$CORE_DIR" ]; then
  echo "error: expected the AHNX-Translation-Core repo cloned at $CORE_DIR (as a sibling of this repo)." >&2
  echo "       git clone https://github.com/AndroidHorizon/AHNX-Translation-Core.git $CORE_DIR" >&2
  exit 1
fi

echo "=== Launcher (this repo) ==="
make

echo "=== Translation Core (x64) ==="
make -C "$CORE_DIR"

echo "=== Translation Core (x32, placeholder) ==="
make -C "$CORE_DIR/core32"

rm -rf testingbuild
mkdir -p testingbuild/AndroidHorizonNX
cp AndroidHorizonNX.nro testingbuild/AndroidHorizonNX.nro
cp "$CORE_DIR/AHNX-Translation-Core-x64.nro" testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x64.nro
cp "$CORE_DIR/core32/AHNX-Translation-Core-x32.nro" testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x32.nro

echo
echo "=== testingbuild/ layout (drag this onto your SD card's /switch/ folder) ==="
find testingbuild -type f
