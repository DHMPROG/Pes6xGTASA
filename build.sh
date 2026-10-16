#!/bin/sh
# Compile dinput8.dll (32 bits) avec le llvm-mingw portable de tools/.
set -e
cd "$(dirname "$0")"
CXX=tools/llvm-mingw/bin/i686-w64-mingw32-clang++
CC=tools/llvm-mingw/bin/i686-w64-mingw32-clang
mkdir -p build/obj

for f in third_party/minhook/src/buffer.c third_party/minhook/src/hook.c \
         third_party/minhook/src/trampoline.c third_party/minhook/src/hde/hde32.c; do
    $CC -O2 -c "$f" -o "build/obj/$(basename "$f" .c).o"
done

IMGUI=third_party/imgui
$CXX -O2 -std=c++17 -static -shared \
    -I third_party/minhook/include -I $IMGUI -I $IMGUI/backends \
    src/main.cpp src/proxy.cpp src/imgui_impl_dx8.cpp src/debugtools.cpp src/fun.cpp\
    $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
    $IMGUI/backends/imgui_impl_win32.cpp \
    build/obj/*.o src/dinput8.def \
    -o build/dinput8.dll -lgdi32 -ldwmapi -luser32
echo "OK -> build/dinput8.dll"
