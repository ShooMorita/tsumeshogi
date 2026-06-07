#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$root_dir/build/native"

if [ "${TSUME_IN_DOCKER:-0}" != "1" ]; then
  exec docker compose -f "$root_dir/docker-compose.yml" run --rm native ./scripts/build_native.sh "$@"
fi

mkdir -p "$build_dir"

clang \
  -Wall \
  -Wextra \
  -std=c11 \
  -I "$root_dir/native/include" \
  -c "$root_dir/native/unity.c" \
  -o "$build_dir/tsume_shogi.o"

printf 'built %s\n' "$build_dir/tsume_shogi.o"
