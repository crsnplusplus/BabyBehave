#!/usr/bin/env bash
# Measure bdd.hpp gcov coverage (both the gtest unit-test suite and the
# self-hosted BabyBehave example) and print the reports at the end.
#
# Usage: scripts/coverage.sh [--style=plain|arrow|tree] [--diagnostics=always|mismatch]
# --style forwards to BABYBEHAVE_STYLE and --diagnostics forwards to
# BABYBEHAVE_DIAGNOSTICS for every step below (ctest and the coverage-bbh
# binaries), since the coverage-bbh CMake target's COMMAND lines are fixed
# and can't take a per-invocation CLI argument - an exported env var is the
# only thing that reaches them without editing CMakeLists.txt.
#
# --style=tree also switches the self-test binaries' Expected/Result/TEST
# blocks to TreeDiagnosticFormatter (Flat otherwise). Those blocks print for
# every scenario by default, not just mismatches; pass --diagnostics=mismatch
# for the old terse "[OK]"/"[FAIL]"-per-scenario behavior instead.
#
# Uses its own build directory (build/coverage) so it never disturbs a
# plain scripts/build.sh build (build/bb-release).

set -euo pipefail

for arg in "$@"; do
	case "${arg}" in
		--style=*)
			export BABYBEHAVE_STYLE="${arg#--style=}"
			;;
		--diagnostics=*)
			export BABYBEHAVE_DIAGNOSTICS="${arg#--diagnostics=}"
			;;
		*)
			echo "coverage.sh: unrecognized argument '${arg}'" >&2
			exit 1
			;;
	esac
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build/coverage"

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
