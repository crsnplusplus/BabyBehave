#!/usr/bin/env bash
# Full build: configure (Release), build, and run the test suite.
#
# Usage: scripts/build.sh [extra cmake configure args...]
# Example: scripts/build.sh -DCMAKE_CXX_COMPILER=clang++

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build"

cmake -S "${repo_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "${build_dir}" -j"$(nproc)"
ctest --test-dir "${build_dir}" --output-on-failure
