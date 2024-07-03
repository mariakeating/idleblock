#!/bin/sh
cd "${0%/*}"

for arg in "$@"; do eval "$arg=1"; done
[ "$release" != "1" ] && debug="1"
[ "$debug" = "1" ]    && release="0" && echo "[debug mode]"
[ "$release" = "1" ]  && debug="0"   && echo "[release mode]"

[ "$clang" != "1" ] && gcc="1"
[ "$gcc" = "1" ]    && clang="0" && echo "[gcc compile]"
[ "$clang" = "1" ]  && gcc="0"   && echo "[clang compile]"

feature_flags=""
[ "$asan" = "1" ] && feature_flags="$feature_flags -fsanitize=address" && echo "[asan enabled]"

compile_common="-ggdb3 -Werror -Wall -Wextra -Wshadow -Wconversion -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable"

gcc_common="$compile_common"
gcc_debug="gcc -O0 $gcc_common"
gcc_release="gcc -O2 $gcc_common"

clang_common="$compile_common"
clang_debug="clang -O0 $clang_common"
clang_release="clang -O2  $clang_common"

[ "$gcc" = "1" ]     && compile_debug="$gcc_debug"
[ "$gcc" = "1" ]     && compile_release="$gcc_release"
[ "$clang" = "1" ]   && compile_debug="$clang_debug"
[ "$clang" = "1" ]   && compile_release="$clang_release"
[ "$debug" = "1" ]   && compile="$compile_debug"
[ "$release" = "1" ] && compile="$compile_release"
compile="$compile $feature_flags"

code="$PWD"/code
mkdir -p build
cd build

$compile "$code"/linux_idleblock.c -o idleblock $(pkgconf --libs --cflags dbus-1)