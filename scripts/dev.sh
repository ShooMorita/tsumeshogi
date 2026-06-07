#!/bin/sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

exec "$root_dir/scripts/dev_web.sh" "$@"
