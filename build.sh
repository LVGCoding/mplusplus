#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target mplusplus --parallel

cd build
exec ./mplusplus "$@"
