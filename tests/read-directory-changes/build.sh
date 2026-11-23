#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:-"$root/out"}
mkdir -p "$out"

: "${X86_64_MINGW_CC:=x86_64-w64-mingw32-gcc}"
: "${I386_MINGW_CC:=i686-w64-mingw32-gcc}"

"$X86_64_MINGW_CC" -O2 -static -o "$out/read_directory_changes.exe" \
    "$root/read_directory_changes_probe.c"
"$I386_MINGW_CC" -O2 -static -o "$out/read_directory_changes32.exe" \
    "$root/read_directory_changes_probe.c"
