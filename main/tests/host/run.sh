#!/bin/sh
# Host tests of the portable iBlue code. Usage: sh main/tests/host/run.sh
set -e
d=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
cc -Wall -Wextra -fsanitize=address,undefined -o "$out/t" "$d/test_iblue_proto.c" "$d/../../system/iblue_cfg_proto.c"
"$out/t"
rm -rf "$out"
