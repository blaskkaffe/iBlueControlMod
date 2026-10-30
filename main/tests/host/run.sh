#!/bin/sh
# Host tests of the portable iBlue code. Usage: sh main/tests/host/run.sh
set -e
d=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/anim" "$d/test_anim.c" "$d/../../system/iblue_anim_curve.c" -lm
"$out/anim"
rm -rf "$out"
