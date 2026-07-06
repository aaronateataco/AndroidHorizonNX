#!/bin/sh
# Builds all three AHNX pieces and arranges them into testingbuild/ matching
# the real SD card layout — drag the contents straight onto your SD card:
#   testingbuild/AndroidHorizonNX.nro                              (the launcher)
#   testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x64.nro    (real engine)
#   testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x32.nro    (placeholder)
#
# Copy testingbuild/AndroidHorizonNX.nro to sdmc:/switch/ and
# testingbuild/AndroidHorizonNX/ to sdmc:/switch/AndroidHorizonNX/ to match
# what the launcher expects (see CORE_X64_PATH/CORE_X32_PATH in
# launcher/source/main.cpp).
set -e
cd "$(dirname "$0")"

echo "=== Translation Core (x64) ==="
make

echo "=== Launcher ==="
make -C launcher

echo "=== Translation Core (x32, placeholder) ==="
make -C core32

rm -rf testingbuild
mkdir -p testingbuild/AndroidHorizonNX
cp launcher/AndroidHorizonNX.nro testingbuild/AndroidHorizonNX.nro
cp AHNX-Translation-Core-x64.nro testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x64.nro
cp core32/AHNX-Translation-Core-x32.nro testingbuild/AndroidHorizonNX/AHNX-Translation-Core-x32.nro

echo
echo "=== testingbuild/ layout (drag this onto your SD card's /switch/ folder) ==="
find testingbuild -type f
