#!/bin/sh
# Prépare l'environnement : sous-modules (MinHook, Dear ImGui) et compilateur llvm-mingw portable dans tools/.
set -e
cd "$(dirname "$0")"
git submodule update --init --recursive

LLVM_MINGW=20260922
if [ ! -x tools/llvm-mingw/bin/i686-w64-mingw32-clang++ ]; then
    mkdir -p tools
    curl -L -o tools/llvm-mingw.zip \
        "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW/llvm-mingw-$LLVM_MINGW-ucrt-x86_64.zip"
    (cd tools && unzip -q llvm-mingw.zip && rm llvm-mingw.zip && mv "llvm-mingw-$LLVM_MINGW-ucrt-x86_64" llvm-mingw)
fi
echo "OK : lance ./build.sh"
