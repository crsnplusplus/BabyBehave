#!/usr/bin/env bash
# Measure bdd.hpp gcov coverage (both the gtest unit-test suite and the
# self-hosted BabyBehave example) and print the reports at the end.
#
# Usage: scripts/coverage.sh
# Uses its own build directory (build-cov) so it never disturbs a plain
# scripts/build.sh build.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build-cov"

cmake -S "${repo_root}" -B "${build_dir}" -DBABYBEHAVE_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "${build_dir}" -j"$(nproc)"
ctest --test-dir "${build_dir}" --output-on-failure

cmake --build "${build_dir}" --target coverage-ut
cmake --build "${build_dir}" --target coverage-bbh

echo
echo "==================== coverage-ut report ===================="
cat "${build_dir}/coverage-ut.txt"
echo
echo "==================== coverage-bbh report ===================="
cat "${build_dir}/coverage-bbh.txt"

# Runs last, after the reports above are already visible, since a
# below-threshold result exits non-zero and would otherwise cut the
# report off.
cmake --build "${build_dir}" --target coverage-gate
