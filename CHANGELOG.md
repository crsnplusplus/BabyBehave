# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
No version of BabyBehave has been tagged/released yet (`git tag` is empty
aside from an unrelated `backup/main-before-author-rewrite` safety tag), so
strictly speaking everything below is still pre-release. See the note at the
bottom of the `[0.7.19]` section for how the two sections below relate to
each other.

## [Unreleased]

Changes present in the working tree but not yet committed, on top of commit
`583034e` ("Harden bdd.hpp and modernize to C++23 with C++17 fallback"),
which is the current tip of `feature/harden-and-modernize`.

### Added

- `TestContext::ContextKey<T>`: an opt-in, compile-time-checked key type for
  `TestContext::Set`/`Get`. Declared once per logical value and passed to
  `Set`/`Get` instead of a raw string, it turns a typo'd key name or a
  wrong-type call site into a compile error instead of a runtime
  `std::bad_any_cast`/`std::out_of_range`. Purely additive sugar over the
  existing string-keyed storage, so it's fully interoperable with the plain
  `Set<T>(string, ...)`/`Get<T>(string)` API. Covered by the new
  `TestContext.testTypedContextKeyRoundTrips` unit test.
- `BABYBEHAVE_NO_SHORT_MACROS`: define before `#include <BabyBehave/bdd.hpp>`
  to skip defining the short fluent macros (`Given`, `With`, `When`, `Then`,
  `And`, `Or`, `But`, and the `...I` variants) when they collide with your
  own or another library's names. The explicit API (`GivenAImpl(...)`,
  `AddStep<StepType>(...)`) is always available regardless. Demonstrated in
  the new `examples/NoShortMacros.cpp`.
- Result-object / collect-all-failures execution mode: `BabyBehaveTest` gains
  `SetCollectFailuresMode(bool = true)`, a public and idempotent `Execute()`
  that now returns `const TestResult&`, and `GetResult()`, backed by two new
  structs, `StepResult` and `TestResult`. When enabled, a failed condition or
  a caught exception in any step no longer invokes the (default `std::exit`)
  failure callbacks; instead the outcome is recorded and execution continues
  with the remaining steps. Off by default, so existing consumers see
  byte-identical behavior. Exercised end-to-end by the new
  `RunCollectFailuresModeScenario` in `examples/SelfTest.cpp`.
- CMake `install()`/`export()`/`find_package()` support: `BabyBehaveLib` is
  now an `INTERFACE` library aliased as `BabyBehave::BabyBehave`, exported as
  `BabyBehaveTargets`, with a generated `BabyBehaveConfig.cmake` (from the
  new `cmake/BabyBehaveConfig.cmake.in`) and
  `BabyBehaveConfigVersion.cmake` (via `write_basic_package_version_file(...
  COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)`), installed under
  `<prefix>/lib/cmake/BabyBehave`. Downstream projects can now
  `find_package(BabyBehave REQUIRED)` after installing, in addition to the
  existing `FetchContent`/`add_subdirectory` vendoring path. The project now
  declares a version: `project(BabyBehave VERSION 0.7.19 LANGUAGES CXX)`
  (previously unversioned) — see the versioning note below.
- Split coverage targets `coverage-ut` and `coverage-bbh` (implemented in the
  new `cmake/coverage_report.cmake`, invoked from the root `CMakeLists.txt`
  when `BABYBEHAVE_ENABLE_COVERAGE` is on): two independent gcov
  measurements of `bdd.hpp`'s header-only inline code — one from the gtest
  unit test suite (`test_TestContext` + `test_BabyBehaveTest`), one from the
  new self-hosted example below — replacing a single combined coverage
  story.
- `example_SelfTest` (`examples/SelfTest.cpp`): a self-hosted "dogfood"
  example — BabyBehave testing BabyBehave through its own fluent API. Nine
  scenarios drive the public surface (happy path; failed
  precondition/action; thrown `std::exception` and non-`std::exception`
  values; a throwing context-setup function; `TestContext` `Set`/`Get`
  round-tripping through a `shared_ptr`; a missing-key throw both directly
  and from inside a step; and the new collect-failures mode) and assert, via
  plain C++ checks in `main()`, that the library behaved as documented.
- `examples/NoShortMacros.cpp`: a minimal example built with
  `BABYBEHAVE_NO_SHORT_MACROS` defined, using only `GivenAImpl`/`AddStep<>`.
- `include/BabyBehave/matchers.hpp` (`BabyBehave::Matchers`): a small,
  standalone, dependency-free fluent-assertion helper — `Expect(value)`
  returns an `Expectation<T>` with `ToEqual`, `ToNotEqual`, `ToBeTrue`,
  `ToBeFalse`, `ToBeGreaterThan`, `ToBeGreaterOrEqualTo`, `ToBeLessThan`,
  `ToBeLessOrEqualTo`, `ToContain`, `ToBeNull`, and `ToNotBeNull`, each
  printing a descriptive actual-vs-expected message to `std::cerr` on
  failure instead of `bdd.hpp`'s generic per-step-type label. It has no
  dependency on `bdd.hpp` (usable standalone) and is called from inside a
  step body like any other helper; used from `bool` step return values, it
  composes with the existing step machinery unmodified. Demonstrated in the
  new `examples/Matchers.cpp`.
- `examples/MultiThreaded.cpp`: demonstrates the safe way to run several
  BabyBehave scenarios concurrently — each `GivenA(...)` owns a private
  `TestContext`, so launching independent scenarios one-per-thread (via
  `std::async`) needs no locking, in contrast to (and directly informed by)
  the `TestContext`-is-not-thread-safe-if-shared note added to `bdd.hpp` in
  the prior commit. Includes an `#if 0`-guarded, never-compiled sketch of
  the unsafe shared-`TestContext` pattern this example deliberately avoids.
  Links `Threads::Threads` via `find_package(Threads REQUIRED)`.
- `.github/workflows/ci.yml`: GitHub Actions CI, manually triggered
  (`workflow_dispatch` only — no automatic push/PR trigger, since a run can
  push a badge-update commit back to `main`), with five jobs —
  `build-and-test` (a `{ubuntu-gcc, ubuntu-clang, macos-clang}` matrix,
  Release build, `ctest`), `build-and-test-windows` (MSVC, its own job
  rather than a matrix cell since the multi-config VS generator needs
  `--config`/`-C` at build/test time instead of `CMAKE_BUILD_TYPE`),
  `clang-tidy` (lints the three shipped headers against the repo-root
  `.clang-tidy`, `WarningsAsErrors: '*'`), `coverage`
  (`BABYBEHAVE_ENABLE_COVERAGE`, runs both `coverage-ut` and `coverage-bbh`
  targets, uploads the resulting reports as artifacts, enforces the
  `coverage-gate` >=90% floor, and — on `main` — commits back
  `.github/badges/coverage.json` and a `.github/badges/build.json` run
  counter that both README badges read via shields.io's `endpoint` badge
  type), and `sanitizers` (`BABYBEHAVE_ENABLE_SANITIZERS`, `ctest` plus
  running every example binary directly so ASan/UBSan can catch anything
  the example binaries would otherwise hide).
- `.clang-tidy`: curated checks (`bugprone-*`, `performance-*`,
  `modernize-*`, `readability-*`, `cppcoreguidelines-*`, `clang-analyzer-*`)
  with six specific checks disabled, each with a documented rationale,
  where the default would fight an established/intentional design choice
  rather than catch a real issue.
- Packaging for easy installation beyond CMake's own `FetchContent`/
  `find_package` story (see the expanded README `## Installation` section):
  a vcpkg overlay port (`ports/babybehave/{vcpkg.json,portfile.cmake}`,
  `--head`-only until a tagged release exists), a Conan 2.x recipe
  (`conanfile.py`, `header-library` package type), and Bzlmod support
  (`MODULE.bazel` + `BUILD.bazel` exposing a `cc_library`, not yet
  published to the Bazel Central Registry).

### Changed

- Examples and tests now link against the exported `BabyBehave::BabyBehave`
  target instead of manually adding
  `target_include_directories(... "${CMAKE_CURRENT_SOURCE_DIR}/../include")`
  in every `add_executable` block.

No fixes in this unreleased batch — the Postcondition-message and
`cmake_minimum_required` bug fixes were already made in the prior commit;
see `[0.7.19]` below.

## [0.7.19] — pre-changelog history (not yet tagged)

This project has no git tags for actual releases, so this section is a
brief retrospective derived from `git log --oneline`, covering everything
up to and including commit `583034e`, i.e. everything *not* listed under
`[Unreleased]` above. It predates the adoption of this changelog, so it's
kept intentionally short rather than itemized like a real changelog entry.

- **Initial skeleton and examples**: project scaffolding, the original
  fluent `Given`/`With`/`When`/`Then`/`And`/`Or`/`But` macros built on
  C++20 concepts, and the first set of examples (Calculator, AlarmClock,
  FlightBooking, Oven, Failing). Several rounds of refactoring,
  "beautification", and a workaround to package the library as header-only.
- **Tests added**: a GoogleTest-based suite under `tests/` (`TestContext`,
  `BabyBehaveTest`) wired up with CTest discovery and `FetchContent`
  (later pointed at a direct GTest download link), plus assorted compiler
  fixes.
- **Various small fixes**: cosmetics passes, a `const`-correctness fix in
  an example, and a non-deprecated CMake minimum-version bump.
- **README**: authored and iterated on independently of the above.
- **This session's hardening + modernization pass** (commit `583034e`,
  "Harden bdd.hpp and modernize to C++23 with C++17 fallback"): fixed a
  mislabeled Postcondition error message (a `Postcondition` step failure
  was being reported as `"Precondition failed"`) and an obsolete
  `cmake_minimum_required(VERSION 3.0)` left over in `src/CMakeLists.txt`;
  hardened exception safety so nothing escapes `BabyBehaveTest`'s
  (`noexcept`) destructor and triggers `std::terminate()` — `catch (...)`
  fallbacks around every step's execution and around invoking
  user-supplied `SetOnConditionNotVerifiedCallback`/`SetOnExceptionCallback`
  callbacks; deleted `BabyBehaveTest`'s copy constructor/assignment to
  prevent double-execution-on-copy; modernized `bdd.hpp` to target C++23
  with a graceful fallback to C++17, selected per-facility via `<version>`
  feature-test macros (`std::move_only_function` falling back to
  `std::function`, `std::println` falling back to `std::cout`, `if
  constexpr` dispatch replacing the earlier C++20-concept-based
  overloads); and added the opt-in CMake options
  `BABYBEHAVE_ENABLE_COVERAGE` (gcov/lcov) and `BABYBEHAVE_ENABLE_SANITIZERS`
  (ASan+UBSan), scoped only to this project's own targets.

**On the version number**: `project(BabyBehave VERSION ...)` was introduced
(in the `[Unreleased]` work above) unversioned-turned-`0.1.0` at first, since
nothing had been tagged before and `0.1.0` is the conventional SemVer
starting point (major version 0 signals "the public API may still change").
That placeholder was superseded by an explicit decision to start the
project's official versioning at `0.7.19` instead — every packaging
manifest (`CMakeLists.txt`, `conanfile.py`, `MODULE.bazel`,
`ports/babybehave/vcpkg.json`) was updated to match, still pre-`v0.7.19`-tag
at the time of this entry.

---

## Maintaining this changelog

When a release is actually cut: bump `project(BabyBehave VERSION x.y.z
...)` in the root `CMakeLists.txt` to match, and turn the relevant
`[Unreleased]` entries into a new dated `## [x.y.z] - YYYY-MM-DD` section
(tag the commit as `vx.y.z` to match). Keep the CMake package version and
this file in sync by hand — there is no automated check enforcing this, so
it relies on doing both in the same commit/PR.
