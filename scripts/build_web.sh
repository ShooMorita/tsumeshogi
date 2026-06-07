#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "${TSUME_IN_DOCKER:-0}" != "1" ]; then
  exec docker compose -f "$root_dir/docker-compose.yml" run --rm web ./scripts/build_web.sh "$@"
fi

cd "$root_dir/web"
npm ci
npm run build:local
