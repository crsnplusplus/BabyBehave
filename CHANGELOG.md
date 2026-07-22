# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This changelog treats `v0.9.0` as the project's first tagged release. As of
this entry the `v0.9.0` git tag does not exist yet — `git tag` is still
empty aside from an unrelated `backup/main-before-author-rewrite` safety
tag — and it is created by hand, after the pull request carrying this
changelog entry merges to `main`, not by any automation in this repository.
Everything below the `[0.9.0]` section, including the entire `[0.7.19]`
section further down, predates the adoption of git tagging for BabyBehave
releases; see the note at the bottom of that `[0.7.19]` section for how it
relates to `[0.9.0]`.

## [0.9.1] - 2026-07-22

### Added

- Ergonomic `TestContext` mutations for in-place object modification:
  `Mutate<T>(key)` returns a live reference into stored values (avoiding
  copy-mutate-writeback ceremony for large objects), and `GetOrInit<T>(key,
  init)` lazily initializes values on first access. Both support string-keyed
  and `Key<T>`-keyed (typed-key) variants for consistency with existing
  `Get<T>`/`Set<T>`. **Important:** these methods are not Gherkin-specific —
  they work with the fluent `Given`/`With`/`When`/`Then` API and everywhere
  else `TestContext` is used.
- `Key<T>` public alias for `TestContext::ContextKey<T>` for brevity when
  declaring typed context keys.
- Bulk declarative step registration via `StepRegistry::RegisterStep()` and
  `RegisterSteps()` (accepting `StepEntry<F>` structures) with the public
  `Gherkin::Keyword` enum (`Given`, `When`, `Then`, `And`, `Or`, `But`) for
  naming steps. Supports PascalCase naming convention for non-trivial step
  functions and inline lambdas for trivial ones, improving readability when
  domains have 10+ steps per registry.
- `StepRegistry::AddAroundHook(tags, before, after)` and
  `AddAroundHookExpr(expression, before, after)` for registering Before+After
  hook pairs in one call instead of two separate registrations.
- Builder-style feature execution via `Gherkin::FeatureRun` class:
  `Feature(text, registry)` and `FeatureFromFile(path, registry)` return a
  builder offering `.Label()`, `.OnFailure()`, `.EnableParallelScenarios()`,
  and `.Run()` methods for named-parameter ergonomics, complementing the
  pre-existing positional `RunFeature()` API.
- `Gherkin::LoadFeatureFile(path)` — a plain filesystem-loading utility
  returning a `std::string`, distinct from the example-only helper in
  `examples/gherkin/LoadFeatureFile.hpp` (which uses CMake-define-based path
  resolution for example `.feature` files).
- `FeatureResult::ExitCode()` returns a portable exit code (0 if
  `allPassed`, non-zero otherwise) for test runners that defer exit-code
  decision to after inspecting results.
- `Gherkin::CollectingFailureHandler` — a thread-safe failure-message
  collector that appends to a `std::vector<std::string>` instead of exiting,
  useful for scenarios that want to gather all failures before deciding on
  exit behavior.
- **Parse-error reporting enhancement (non-breaking):** Structural parse
  errors across an entire `.feature` file are now ALL collected and reported
  in one pass (one `onFailure` call per error, format `"<file>:<line>: parse
  error: <message>"`) instead of stopping at the first one. The hard invariant
  is unchanged: if ANY structural error exists, ZERO scenarios execute.
  Error recovery is now more sophisticated (point errors, corruptive errors
  with resync, and intermediate errors with partial skipping) to avoid
  cascading bogus errors from corrupted parse state.
- **Scenario-failure reporting enhancement (non-breaking):** Scenario failure
  messages are now a single concise line
  (`"<file>:<scenarioLine>: scenario failed: '<name>' - K/N step(s) failed,
  first: [<stepLabel>] <stepName>: <message> (at <location>)"`) instead of a
  multi-line enumeration. Full per-step detail remains completely available
  via `FeatureResult::scenarioResults[i].steps` — the digest is only what
  reaches the `onFailure` callback, making it easier to skim failure
  summaries in large test runs.
- Fixed an inaccurate comment in `.github/workflows/ci.yml` (lines 226–231)
  claiming that Gherkin example binaries are "ctest-registered" — they are
  actually run via a plain shell loop with an allow-list of expected-to-fail
  binaries.

### Changed

- (none — this is an additive, non-breaking release)

### Fixed

- (none)

## [0.9.0] - 2026-07-21

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
  `RunCollectFailuresModeScenario` in the self-hosted suite described below
  (`tests/bdd/test_SelfTest.cpp`).
- CMake `install()`/`export()`/`find_package()` support: `BabyBehaveLib` is
  now an `INTERFACE` library aliased as `BabyBehave::BabyBehave`, exported as
  `BabyBehaveTargets`, with a generated `BabyBehaveConfig.cmake` (from the
  new `cmake/BabyBehaveConfig.cmake.in`) and
  `BabyBehaveConfigVersion.cmake` (via `write_basic_package_version_file(...
  COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)`), installed under
  `<prefix>/lib/cmake/BabyBehave`. Downstream projects can now
  `find_package(BabyBehave REQUIRED)` after installing, in addition to the
  existing `FetchContent`/`add_subdirectory` vendoring path. The project
  first declared a version with `project(BabyBehave VERSION 0.7.19
  LANGUAGES CXX)` (previously unversioned) — see the versioning note below
  and, further down in this section, the subsequent bump to `0.9.0`.
- Split coverage targets `coverage-ut` and `coverage-bbh` (implemented in the
  new `cmake/coverage_report.cmake`, invoked from the root `CMakeLists.txt`
  when `BABYBEHAVE_ENABLE_COVERAGE` is on): two independent gcov
  measurements of `bdd.hpp`'s header-only inline code — one from the gtest
  unit test suite (`test_TestContext` + `test_BabyBehaveTest`), one from the
  self-hosted example below — replacing a single combined coverage story.
- `test_bdd_SelfTest` (`tests/bdd/test_SelfTest.cpp`, registered with CTest
  as the `SelfTest` test): a self-hosted "dogfood" example — BabyBehave
  testing BabyBehave through its own fluent API. Nine scenarios drive the
  public surface (happy path; failed precondition/action; thrown
  `std::exception` and non-`std::exception` values; a throwing
  context-setup function; `TestContext` `Set`/`Get` round-tripping through a
  `shared_ptr`; a missing-key throw both directly and from inside a step;
  and the collect-failures mode above) and assert, via plain C++ checks in
  `main()`, that the library behaved as documented. (This example was
  originally located directly under `examples/` and was later relocated
  into `tests/bdd/`, gaining the `test_bdd_SelfTest` CMake target name, as
  part of wiring the Gherkin runtime into the coverage gates further below
  in this section.)
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
  push a badge-update commit back to `main`), initially with five jobs —
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
  the example binaries would otherwise hide). A sixth job, `gherkin`
  (`gherkin (interpreter and examples)`), was added later, once the Gherkin
  work below landed: it builds and `ctest`s the Release configuration and
  then also runs every `example_Gherkin*` binary directly, tolerating
  non-zero exit from the handful of examples (`example_GherkinUnmatchedStep`,
  `example_GherkinCollectFailures`, `example_GherkinCustomFailureHandler`,
  `example_GherkinBakeryLateCancellation`) that deliberately demonstrate a
  failing path.
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
- Gherkin runtime interpreter support added directly to `bdd.hpp` (developed
  internally as "v0.8.0"/"v0.8.1", never tagged as such — it lands here as
  part of `[0.9.0]`): a runtime (not code-generating) interpreter for
  Gherkin `.feature` text, gated behind `#if
  !defined(BABYBEHAVE_DISABLE_GHERKIN)` and opt-out via the new
  `BABYBEHAVE_DISABLE_GHERKIN` macro. Adds `StepRegistry` for registering
  `Given`/`When`/`Then` step definitions and hooks, support for `Background:`
  blocks (steps prepended to every `Scenario` in the `Feature`), `@tag`
  annotations at the `Feature`/`Scenario` level, and tag-filtered
  `AddBeforeHook`/`AddAfterHook` hooks run around each scenario. Step text
  can capture typed arguments via a small cucumber-expression-like
  placeholder syntax (`{int}`, `{float}`, `{string}`, `{word}`). `RunFeature`
  takes a `GherkinFailureCallback` (`onFailure` parameter, default
  `impl::DefaultGherkinFailureAction`, which prints to `stderr` and exits) as
  its extensibility point for collecting rather than exiting on failures.
  Ten new example programs exercise the interpreter (`GherkinAdvanced`,
  `GherkinBackground`, `GherkinBasket`, `GherkinCollectFailures`,
  `GherkinCustomFailureHandler`, `GherkinMultiThreaded`,
  `GherkinPlaceholders`, `GherkinTagsAndHooks`, `GherkinUnmatchedStep`,
  `GherkinVeryAdvanced`) alongside new design docs,
  `docs/design/gherkin-support.md` and `docs/configuration.md` (the latter
  also documenting the pre-existing `BABYBEHAVE_NO_SHORT_MACROS`,
  `BABYBEHAVE_QUIET`, and `BABYBEHAVE_STYLE` knobs).
- `StepRegistry::Merge(const StepRegistry& other)`, plus explicitly
  defaulted copy/move construction and assignment on `StepRegistry` (it was
  previously move-only in practice), so a base set of step definitions can
  be composed with feature-specific additions instead of duplicated. Put to
  use across fifteen new example programs and their accompanying `.feature`
  files under the new `examples/gherkin/` directory, split across two
  domains — bakery (`BakerySteps.hpp` shared across
  `GherkinBakeryAllergenSubstitution`, `GherkinBakeryBulkOrderItemized`,
  `GherkinBakeryConcurrentOrderProcessing`, `GherkinBakeryDailyOvenLifecycle`,
  `GherkinBakeryFlakyOvenSensorRetry`, `GherkinBakeryLateCancellation`,
  `GherkinBakerySeasonalDiscountTiers`, `GherkinBakeryStandardOrder`) and
  library (`LibrarySteps.hpp` shared across
  `GherkinLibraryBookReviewSubmission`, `GherkinLibraryConcurrentLending`,
  `GherkinLibraryHoldPickupDeadline`, `GherkinLibraryHoldsAndReservations`,
  `GherkinLibraryOverdueFines`, `GherkinLibraryPriorityPatronHandling`,
  `GherkinLibraryStandardLending`) — including
  `GherkinLibraryConcurrentLending`, which runs several scenarios sharing
  one merged registry concurrently via `std::async`.
- Gherkin language/runtime extensions beyond plain Gherkin, added alongside
  the bakery/library examples above: `Scenario Outline`/`Examples:` (and the
  `Scenarios:` alias) blocks, expanding `<placeholder>` substitutions across
  however many `Examples:`/`Scenarios:` blocks are declared, in declaration
  order; Data Tables and Doc Strings as opt-in step parameters (a step
  function can declare a trailing `const DataTable&` and/or `const
  std::string&` parameter to receive a pipe-delimited table or a
  `"""`-delimited doc string attached to that step); `@timeout:<value><unit>`
  (`s`/`ms`/`m`) tags enforcing a cooperative, non-preemptive wall-clock
  deadline checked between steps (`After` hooks still run on timeout);
  `@retry:N` tags automatically re-running a scenario up to `N` times with a
  fresh `TestContext` per attempt, reporting only the final outcome; boolean
  tag expressions (`and`/`or`/`not`, with parentheses) for hook
  registration via the new `AddBeforeHookExpr`/`AddAfterHookExpr`; an
  `enableParallelScenarios` flag on `RunFeature` to run a `Feature`'s
  `Scenario`s concurrently via `std::async` while still reporting results in
  declaration order (requires a non-exiting `onFailure` callback); and
  suite-level `AddBeforeAllHook`/`AddAfterAllHook`, run exactly once per
  `Feature` regardless of tags.

### Changed

- Examples and tests now link against the exported `BabyBehave::BabyBehave`
  target instead of manually adding
  `target_include_directories(... "${CMAKE_CURRENT_SOURCE_DIR}/../include")`
  in every `add_executable` block.
- The project version — `project(BabyBehave VERSION ...)` in the root
  `CMakeLists.txt`, and the matching version strings in `conanfile.py`,
  `MODULE.bazel`, and `ports/babybehave/vcpkg.json` — was bumped from
  `0.7.19` to `0.9.0` so all packaging manifests agree with this being the
  project's first tagged release (see the preamble at the top of this file
  and the versioning note at the bottom of the `[0.7.19]` section below).
- Seven of the ten Gherkin example programs added above
  (`GherkinAdvanced`, `GherkinCollectFailures`, `GherkinCustomFailureHandler`,
  `GherkinMultiThreaded`, `GherkinPlaceholders`, `GherkinUnmatchedStep`,
  `GherkinVeryAdvanced`) moved into the new `examples/gherkin/` directory
  alongside the bakery/library examples; `GherkinBackground`,
  `GherkinBasket`, and `GherkinTagsAndHooks` stayed in `examples/`.
- Every Gherkin example's `main()` was retrofitted to a consistent
  `PrepareRegistry()` (builds and returns a `StepRegistry`) /
  `RunFeatureFromFile(...)` (loads and runs the `.feature` file against
  that registry) convention, separating registry construction from feature
  execution instead of interleaving them inline in `main()`.
- The `tests/bdd/test_SelfTest_Gherkin.cpp` and
  `tests/bdd/test_SelfTest_Gherkin_DefaultFailureExit.cpp` suites were wired
  into the `coverage-ut`/`coverage-bbh` gates above — the Gherkin runtime in
  `bdd.hpp` had previously been invisible to those coverage measurements.
  Combined with the `ParseNarrationStyleEnv()` fix below, this took
  `coverage-bbh`/`coverage-ut` to 100% at that checkpoint; the subsequent
  Scenario Outline/Data Table/Doc String/`@timeout`/`@retry`/tag-expression/
  parallel-scenario/suite-hook code added above brought `bdd.hpp`'s measured
  coverage back down to 98.47%/98.45% (`coverage-bbh`/`coverage-ut`) simply
  because that much new code shipped after the 100% checkpoint was reached,
  not because of a regression in already-covered code.
- Build output directories were consolidated under
  `build/{bb-release,coverage,clang-tidy,bb-debug}/` (previously a plain
  `build/` plus an ad hoc `build-cov/` and other one-off directories),
  updated consistently across `scripts/build.sh`, `scripts/coverage.sh`, all
  six CI jobs above, `README.md`, and `docs/configuration.md`; `.gitignore`
  was simplified to match.
- `README.md`'s remaining references to the self-hosted example's old
  location under `examples/` were updated to point at
  `tests/bdd/test_SelfTest.cpp`.

### Fixed

- `JoinNarrationLines()` in `bdd.hpp` no longer rebuilds the narration
  string from scratch for every appended line; it accumulates directly into
  a single `std::string` via `+=`, turning an O(n²) join into a linear one.
- `cmake/coverage_report.cmake` no longer counts a source line that
  consists solely of a closing brace (`}` or `};`) as an uncovered-line
  coverage gap — gcov instruments a function's closing brace with its own
  "reached end of function" counter, separate from the statement above it,
  so a brace-only line can never independently execute any logic of its own
  and is now treated like gcov's own `-` (non-executable) marker.
- Closed a coverage gap in `bdd.hpp`'s `ParseNarrationStyleEnv()` by adding
  test variants for `BABYBEHAVE_STYLE=arrow`, `=tree`, and an invalid
  (`bogus`) value, exercising the fallback-to-plain branch that was
  previously untested.
- Stale references in `README.md` to the self-hosted example's old
  `examples/`-based location were corrected to `tests/bdd/test_SelfTest.cpp`
  (see Changed above); a similar stale reference survives in a comment in
  `include/BabyBehave/reporters.hpp` and is left for a follow-up pass.

The Postcondition-mislabeled-as-Precondition error message and the obsolete
`cmake_minimum_required(VERSION 3.0)` bug fixes predate this section and are
already documented under `[0.7.19]` below.

## [0.7.19] — pre-changelog history (not yet tagged)

This project has no git tags for actual releases, so this section is a
brief retrospective derived from `git log --oneline`, covering everything
up to and including commit `583034e`, i.e. everything *not* listed under
`[0.9.0]` above. It predates the adoption of this changelog, so it's
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
(in the `[0.9.0]` work above) unversioned-turned-`0.1.0` at first, since
nothing had been tagged before and `0.1.0` is the conventional SemVer
starting point (major version 0 signals "the public API may still change").
That placeholder was superseded by an explicit decision to start the
project's official versioning at `0.7.19` instead — every packaging
manifest (`CMakeLists.txt`, `conanfile.py`, `MODULE.bazel`,
`ports/babybehave/vcpkg.json`) was updated to match, still pre-`v0.7.19`-tag
at the time of this entry. `0.7.19` itself was later superseded in turn by
a further decision to skip straight to tagging `v0.9.0` as the project's
first release instead of `v0.7.19`; see the version-bump entry under
`[0.9.0]`'s Changed section above for that follow-up.

---

## Maintaining this changelog

When a release is actually cut: bump `project(BabyBehave VERSION x.y.z
...)` in the root `CMakeLists.txt` to match, and turn the relevant
`[Unreleased]` entries into a new dated `## [x.y.z] - YYYY-MM-DD` section
(tag the commit as `vx.y.z` to match). Keep the CMake package version and
this file in sync by hand — there is no automated check enforcing this, so
it relies on doing both in the same commit/PR.
