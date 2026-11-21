#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 PROBE_OUTPUT" >&2
    exit 2
fi

output=$1
test -s "$output"

# The object names contain a run-specific private namespace path, so assert the
# API contract and stable Win32 errors instead of diffing those paths literally.
if grep -q '^missing ' "$output"; then
    echo "private-namespace export missing:" >&2
    grep '^missing ' "$output" >&2
    exit 1
fi
grep -Eq '^\[parent\] Create\(B1, "P"\)[[:space:]]+ok[[:space:]]+err=3735928559' "$output"
grep -Eq '^\[parent\] CreateMutex "P\\m"[[:space:]]+ok[[:space:]]+err=0' "$output"
grep -Eq '^\[parent\] OpenMutex "p\\m" \(alias in lower case\)[[:space:]]+ok[[:space:]]+err=3735928559' "$output"
grep -Eq '^\[parent\] CreateMutex "Z\\m" \(no such alias\)[[:space:]]+NULL[[:space:]]+err=3' "$output"
grep -Eq '^\[parent\] Close\(P3, DESTROY\)[[:space:]]+ret=1' "$output"
