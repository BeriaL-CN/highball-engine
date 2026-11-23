#!/bin/sh
set -eu

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo "usage: $0 WINE_BIN WINEPREFIX OUTPUT_DIR [PROBE]" >&2
    exit 2
fi

wine_bin=$1
prefix=$2
out=$3
probe=${4:-read_directory_changes.exe}
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$out"

WINEPREFIX="$prefix" "$wine_bin" "$root/out/$probe" >"$out/$probe.txt" 2>&1
