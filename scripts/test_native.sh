#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$root_dir/build/tests"

if [ "${TSUME_IN_DOCKER:-0}" != "1" ]; then
  exec docker compose -f "$root_dir/docker-compose.yml" run --rm native ./scripts/test_native.sh "$@"
fi

mkdir -p "$build_dir"

for test_name in parser movegen solver; do
  clang \
    -Wall \
    -Wextra \
    -std=c11 \
    -I "$root_dir/native/include" \
    -I "$root_dir/native/tests" \
    "$root_dir/native/unity.c" \
    "$root_dir/native/tests/${test_name}_test.c" \
    -o "$build_dir/${test_name}_test"

  "$build_dir/${test_name}_test"
done
