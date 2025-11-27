#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

export CCACHE_CPP2=yes

echo "==> cmake configure --preset fast-debug"
cmake --preset fast-debug

NPROCS="$(nproc || echo 2)"
echo "==> build (ninja -j${NPROCS})"

cmake --build --preset fast-debug -- -j"${NPROCS}"

echo
echo "Build finished successfully."
echo "Executables are located in: build/fast-debug/bin/"