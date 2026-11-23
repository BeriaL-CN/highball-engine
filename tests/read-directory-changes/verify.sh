#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 PROBE_OUTPUT" >&2
    exit 2
fi

output=$1
test -s "$output"
grep -q '^create: PASS$' "$output"
grep -q '^modify: PASS$' "$output"
grep -q '^delete: PASS$' "$output"
grep -Eq '^rename: PASS( \(removed\+added\))?$' "$output"
grep -q '^no-change: PASS$' "$output"
