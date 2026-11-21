#!/bin/sh
set -eu

if [ "$#" -lt 3 ]; then
    echo "usage: $0 WINE_BIN WINEPREFIX OUTPUT_DIR [probe.exe]" >&2
    exit 2
fi

wine_bin=$1
prefix=$2
out=$3
probe=${4:-private_ns_probe.exe}
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$out"

WINEPREFIX="$prefix" "$wine_bin" "$root/out/$probe" >"$out/$probe.txt" 2>&1

