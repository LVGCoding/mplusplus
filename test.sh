#!/usr/bin/env bash
# Build and run the M++ unit tests. Extra args are passed to ctest,
# e.g. ./test.sh -R Parser   (only run tests whose name matches "Parser")
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel --target mplusplus_tests
ctest --test-dir build --output-on-failure "$@"
