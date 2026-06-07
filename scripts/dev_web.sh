#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "${TSUME_IN_DOCKER:-0}" != "1" ]; then
  compose_file="$root_dir/docker-compose.yml"

  docker compose -f "$compose_file" run --rm native ./scripts/test_native.sh
  docker compose -f "$compose_file" run --rm native ./scripts/build_native.sh
  docker compose -f "$compose_file" run --rm wasm ./scripts/build_wasm.sh
  exec docker compose -f "$compose_file" run --rm --service-ports web ./scripts/dev_web.sh "$@"
fi

cd "$root_dir/web"
npm ci
npm run build:local
npm run dev:local -- "$@"
