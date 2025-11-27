#!/usr/bin/env bash
set -euo pipefail

echo "==> Setup ccache for fast builds (Ubuntu)"
if ! command -v ccache >/dev/null 2>&1; then
  echo "ccache not found. Installing (requires sudo)..."
  sudo apt update
  sudo apt install -y ccache
else
  echo "ccache detected."
fi

# Recommend config
echo
echo "Recommended: enable ccache compiler launcher in CMake presets (already set in provided presets)."
echo "You can inspect ccache stats with: ccache -s"

# Optionally ensure symlinks exist (clang/g++ redirect)
echo
echo "Optional: create symlinks so ccache intercepts compilers (may require sudo):"
echo " sudo ln -sf $(which ccache) /usr/local/bin/clang++"
echo " sudo ln -sf $(which ccache) /usr/local/bin/g++"
echo
echo "Finish. Run ./scripts/build_fast.sh to configure & build (fast-debug)."
