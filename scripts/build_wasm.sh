#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out_dir="$root_dir/web/src/wasm"

if [ "${TSUME_IN_DOCKER:-0}" != "1" ]; then
  exec docker compose -f "$root_dir/docker-compose.yml" run --rm wasm ./scripts/build_wasm.sh "$@"
fi

mkdir -p "$out_dir"

emcc \
  -Wall \
  -Wextra \
  -std=c11 \
  -I "$root_dir/native/include" \
  "$root_dir/native/unity.c" \
  -sMODULARIZE=1 \
  -sEXPORT_ES6=1 \
  -sENVIRONMENT=web \
  -sSTACK_SIZE=8388608 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_FUNCTIONS='["_tsume_solve_json","_tsume_free","_malloc","_free"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8"]' \
  -o "$out_dir/tsume_shogi.js"
