#!/bin/sh
# Host tests of the portable iBlue code, plus a check that the two copies of the protocol header are
# the same. Usage: sh main/tests/host/run.sh
set -e
d=$(cd "$(dirname "$0")" && pwd)
root="$d/../../.."
out=$(mktemp -d)
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/hotkey" "$d/test_hotkey.c" "$d/../../iblue/hotkey.c"
"$out/hotkey"
cmp "$root/main/iblue/brctl_proto.h" "$root/dc/brctl/brctl_proto.h" || {
    echo "main/iblue/brctl_proto.h and dc/brctl/brctl_proto.h differ: copy the first over the second" >&2
    exit 1
}
echo "brctl_proto.h copies match"
rm -rf "$out"
