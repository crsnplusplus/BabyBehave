# BabyBehave

![CI](https://github.com/crsnplusplus/BabyBehave/actions/workflows/ci.yml/badge.svg)
![Coverage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/crsnplusplus/BabyBehave/main/.github/badges/coverage.json)
![Build](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/crsnplusplus/BabyBehave/main/.github/badges/build.json)

A minimalistic, header-only BDD framework for modern C++.

Write behavior-driven tests as plain C++ functions and compose them with a fluent `Given / With / When / Then` API. No dependencies, no code generation — the test *is* the specification, and the readable output comes for free from your function names. Gherkin support (runtime `.feature` file interpreter) is included by default, but you are never *required* to use it: plain C++ functions are still the primary, zero-ceremony way to write specs. For pure C++17 builds or consumers who explicitly don't want Gherkin, define `BABYBEHAVE_DISABLE_GHERKIN` before including the header.

```text
Given a: FreshlyBootedCoffeeMachine
    With: AFullWaterTank
    When: IBrewAnEspresso
    Then: ACupIsServed
    And: TheTankLevelDecreases
```

## Features

- **Header-only**: drop `include/BabyBehave/bdd.hpp` into your project and you're done
- **C++23, with graceful C++17 fallback**: the header uses `<version>` feature-test macros to pick the best available standard library facility at each call site (see [C++ standard support](#c-standard-support) below) — no hard C++23 requirement on the header itself
- **Zero dependencies**: standard library only
- **Fluent BDD vocabulary**: `Given`, `With`, `When`, `Then`, `And`, `Or`, `But` (plus `GivenA`, `WithI`, `WhenI`, `ThenI`, … variants for readable English), with an opt-out for consumers whose codebase already uses those names (see [Customizing the macros](#customizing-the-macros))
- **Shared test context**: a `std::any`-backed key/value store passes state between steps, with an optional compile-time-checked key type for consumers who want to avoid stringly-typed lookups (see [TestContext](#testcontext))
- **Customizable failure handling**: plug in your own callbacks for failed conditions and exceptions
- **Exception-safe by construction**: failures inside steps, context setup, or your own callbacks are all caught and routed safely — nothing escapes into the (`noexcept`) destructor and triggers `std::terminate()`
- **Result objects when you want them**: opt into `SetCollectFailuresMode(true)` and every step's outcome is collected into a `TestResult` instead of stopping at the first failure (see [Collecting results instead of exiting](#collecting-results-instead-of-exiting))
- **Multi-assertion steps**: `SoftCheck` lets one step record several named sub-checks that aggregate into a single readable failure message (see [Soft checks: multiple assertions per step](#soft-checks-multiple-assertions-per-step))
- **Call-site diagnostics**: with `std::source_location` support, every step's call site is captured automatically and surfaced in failure messages and `StepResult::location`, with zero source changes required (see [Call-site diagnostics](#call-site-diagnostics))
- **Fluent matchers** (optional, standalone `matchers.hpp`): `Expect(value).ToEqual(...)`, `.ToBeTrue()`, and friends for descriptive actual-vs-expected failure messages (see [Fluent matchers](#fluent-matchers))
- **Structured CI output** (optional `reporters.hpp`): serialize collected results to JUnit XML or TAP for your CI dashboard of choice (see [Structured output: JUnit XML / TAP](#structured-output-junit-xml--tap))
- **CMake install/export support**: consume it via `FetchContent`/`add_subdirectory` *or* `find_package(BabyBehave REQUIRED)` after installing it (see [Installation](#installation))
- **MIT licensed**

## Quick start

A step is any function taking a `TestContext&` and returning `bool`. The context setup function (used by `Given`) returns `void`.

```cpp
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

void FreshlyBootedCoffeeMachine(TestContext& ctx) {
    ctx.Set("machine", std::make_shared<CoffeeMachine>());
}

bool AFullWaterTank(TestContext& ctx) {
    ctx.Get<std::shared_ptr<CoffeeMachine>>("machine")->FillTank();
    return true;
}

bool IBrewAnEspresso(TestContext& ctx) {
    return ctx.Get<std::shared_ptr<CoffeeMachine>>("machine")->Brew();
}

bool ACupIsServed(TestContext& ctx) {
    return ctx.Get<std::shared_ptr<CoffeeMachine>>("machine")->CupsServed() == 1;
}

int main() {
    Given(FreshlyBootedCoffeeMachine)
        .With(AFullWaterTank)
        .When(IBrewAnEspresso)
        .Then(ACupIsServed);
}
```

The macros stringify the function names, so the scenario above prints itself as the spec shown at the top — naming your steps well *is* writing your documentation.

## How it works

- `Given(fn)` creates a `BabyBehaveTest` and runs `fn` to set up the `TestContext`.
- Each chained step (`With`, `When`, `Then`, `And`, `Or`, `But`) registers a typed step (`Precondition`, `Action`, `Postcondition`, `And`, `Or`, `But`) via `AddStep<StepType>(name, fn)`.
- The scenario executes when the test object goes out of scope; steps run in order and each one's return value is verified.
- Step types are stored in a `std::variant` and dispatched with `std::visit` + `if constexpr`, so each keyword gets its own labelled output ("With:", "When:", "Then:", …) and failure message.

## TestContext

`TestContext` is a `std::any`-based store shared across all steps of a scenario. There are two ways to use it:

**Plain string keys** — quick to write, but a typo'd key or a mismatched type only fails at runtime (`std::out_of_range` if the key is missing, `std::bad_any_cast` if the type doesn't match):

```cpp
ctx.Set("answer", 42);
int x = ctx.Get<int>("answer");
```

**Typed `ContextKey<T>`** — an opt-in, compile-time-checked key you declare once per logical value. It's purely additive sugar over the same underlying string-keyed storage, so it interoperates freely with the plain API above:

```cpp
static constexpr BabyBehave::BDD::TestContext::ContextKey<int> kAnswerKey{"answer"};

ctx.Set(kAnswerKey, 42);
int x = ctx.Get(kAnswerKey);   // wrong-type Set/Get calls fail to compile instead of throwing
```

## Customizing the macros

The fluent keywords (`Given`, `With`, `When`, `Then`, `And`, `Or`, `But`, and their `...I` variants) are implemented as macros so they can stringify your function names. They use capitalized identifiers to stay clear of the C++ alternative tokens `and`/`or`, but a name like `And` or `When` can still collide with another library or with your own code.

If that happens, define `BABYBEHAVE_NO_SHORT_MACROS` before including the header to skip defining the short macros entirely, and call the underlying API directly instead: `GivenAImpl("name", fn)` to start a scenario and `AddStep<StepType>("name", fn)` (with `StepType` one of `Precondition`, `Action`, `Postcondition`, `And`, `Or`, `But`) to add each step. See [`examples/NoShortMacros.cpp`](examples/NoShortMacros.cpp) for a complete, working example of this style:

```cpp
#define BABYBEHAVE_NO_SHORT_MACROS
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

int main() {
    GivenAImpl("an empty basket", EmptyBasket)
        .AddStep<Precondition>("BasketIsEmpty", BasketIsEmpty)
        .AddStep<Action>("AddItemToBasket", AddItemToBasket)
        .AddStep<Postcondition>("BasketHasOneItem", BasketHasOneItem);
}
```

## Failure handling

By default a failed condition or an uncaught exception prints an error and exits with `EXIT_FAILURE`. Both behaviors are injectable, which makes BabyBehave easy to embed in another test runner:

```cpp
auto test = Given(FreshlyBootedCoffeeMachine);
test.SetOnConditionNotVerifiedCallback([](const std::string& msg) {
    /* report to your framework instead of exiting */
});
test.SetOnExceptionCallback([](const std::string& step, const std::exception& e) {
    /* ... */
});
```

Exceptions are contained at every boundary: a throw from context setup, from any step, or even from your own `SetOnConditionNotVerifiedCallback`/`SetOnExceptionCallback` is caught and never allowed to propagate out of `BabyBehaveTest`'s destructor (which is where scenario execution actually happens). Non-`std::exception` throws (e.g. `throw 42;`) are caught too and reported with a generic message instead of crashing the process. [`tests/bdd/test_SelfTest.cpp`](tests/bdd/test_SelfTest.cpp) exercises every one of these paths directly — see [Self-hosted dogfood example](#self-hosted-dogfood-example) below.

### Call-site diagnostics

On toolchains with `<source_location>` support (guarded by `__cpp_lib_source_location`, the same feature-test-macro pattern as the C++23 facilities in [C++ standard support](#c-standard-support) below), every `AddStep<StepType>(...)` call — including the ones the `With`/`When`/`Then`/`And`/`Or`/`But` macros expand to, and the `Given`/`GivenA` context-setup call — automatically captures its own call site via a `std::source_location::current()` default parameter, with zero source changes needed on your part. The formatted `"file:line"` shows up in two places:

- Appended to the failure message passed to `SetOnConditionNotVerifiedCallback`/`SetOnExceptionCallback` (and to the default `std::exit`-ing callbacks) as `" (at file:line)"`, pointing straight at the failing `With(...)`/`When(...)`/`Then(...)`/... line.
- In `StepResult::location`, for every step (pass or fail), when running under `SetCollectFailuresMode(true)` (see [Collecting results instead of exiting](#collecting-results-instead-of-exiting)); `reporters.hpp`'s `ToJUnitXml()` uses it to populate `<testcase file="..." line="...">`.

On toolchains without `<source_location>`, `location` is always the empty string and no message suffix is appended — everything else behaves identically.

### Self-hosted dogfood example

[`tests/bdd/test_SelfTest.cpp`](tests/bdd/test_SelfTest.cpp) is BabyBehave testing BabyBehave, using its own `Given`/`With`/`When`/`Then` API to drive scenarios and check that the library behaves the way its contract promises (happy path, failed preconditions/actions, thrown `std::exception`s, thrown non-`std::exception` values, context-setup exceptions, `TestContext` round-tripping, collect-failures mode, and `SoftCheck`).

Because the default failure callbacks call `std::exit()`, and `BabyBehaveTest::Execute()` runs from the destructor, `test_SelfTest.cpp` installs its own `SetOnConditionNotVerifiedCallback`/`SetOnExceptionCallback` on every scenario to *record* the outcome instead of exiting, then inspects what was recorded once each scenario's `BabyBehaveTest` goes out of scope. This is the same pattern you'd use to embed BabyBehave scenarios inside your own test harness (gtest, Catch2, a CI script, …) instead of letting a failure kill the whole process.

## Collecting results instead of exiting

By default, a failed step invokes `SetOnConditionNotVerifiedCallback`/`SetOnExceptionCallback` (which default to printing to `std::cerr` and calling `std::exit(EXIT_FAILURE)`), and execution stops at the first failure. `BabyBehaveTest::SetCollectFailuresMode(true)` switches to a different mode: every step still runs in order, its outcome is appended to an internal `TestResult`, and execution *continues* with the rest of the chain instead of stopping or invoking the failure callbacks at all.

```cpp
struct StepResult {
    std::string stepLabel;   // "Precondition", "Action", "Postcondition", "And", "Or", "But", or "ContextSetup"
    std::string stepName;    // the function name captured by With/When/Then/...
    bool passed = true;
    std::string message;     // empty when passed; the failure/exception message otherwise
    std::string location;    // "file:line" of the call site (see Call-site diagnostics above)
};

struct TestResult {
    std::string testName;
    bool allPassed = true;   // AND of every recorded StepResult::passed
    std::vector<StepResult> steps;
};
```

`Execute()` is what actually runs the scenario (context setup, then every step) — it's what the destructor calls implicitly, but it's also public and idempotent: the first call runs the scenario and caches the `TestResult`, and any later call (including the implicit one from the destructor) just returns the cached result. Because the destructor is what normally triggers execution, and the `BabyBehaveTest` object is gone by the time its destructor returns, a consumer who wants the `TestResult` can't rely on the usual fire-and-forget macro chain — it has to bind the test to a named variable and call `Execute()` explicitly while the object is still alive:

```cpp
BabyBehaveTest test = GivenA(SetupTrivialContext);
test.SetCollectFailuresMode(true);
test.With(StepPreconditionFalseForCollect)
    .When(StepActionTrueForCollect)
    .And(StepAndThrowsForCollect)
    .Then(StepThenTrueForCollect);

const TestResult& result = test.Execute();
// result.allPassed is false; result.steps has one StepResult per step, in
// order, including the ones that passed (test's destructor is now a no-op).
```

(adapted from `RunCollectFailuresModeScenario` in [`tests/bdd/test_SelfTest.cpp`](tests/bdd/test_SelfTest.cpp); `GetResult()` returns the same `TestResult` without re-running anything, for inspecting it again later.)

Off by default, so a consumer who never calls `SetCollectFailuresMode(true)` sees byte-identical behavior to before this feature existed.

## Soft checks: multiple assertions per step

A step is still a single `bool(TestContext&)`, but sometimes you want to check several independent things in one step and see all of them in the failure message, not just a generic "Action failed". `SoftCheck` is an opt-in recorder for that:

```cpp
bool StepActionWithSoftChecks(TestContext& context) {
    SoftCheck checks(context);
    const int count = 15;
    checks.Check("has valid id", true);
    checks.Check("name matches", true);
    checks.Check("count in range", count >= 1 && count <= 10, "count was " + std::to_string(count));
    return checks.AllPassed();
}
```

`Check(label, condition, message = "")` records one named sub-check and returns `condition` unchanged (handy for early-return patterns); `AllPassed()` is the AND of every `Check()` call made so far (`true` if `Check()` was never called, so a step that only conditionally checks is unaffected). If the step's overall `bool` comes back `false` **and** at least one sub-check failed, the failure message is extended with the failing sub-checks — for the snippet above, that's `"Action failed: count in range (count was 15)"`. Passing sub-checks are omitted from the message; only the failing ones are useful. This applies both to the default failure-callback path and to `SetCollectFailuresMode(true)`'s `StepResult::message` — see [`tests/bdd/test_SelfTest.cpp`](tests/bdd/test_SelfTest.cpp)'s `RunSoftCheckCollectFailuresScenario`/`RunSoftCheckDefaultCallbackScenario` for both. A step that never constructs a `SoftCheck` is completely unaffected — this is purely additive.

`condition` is a plain, eagerly-evaluated `bool`, so `SoftCheck::Check` composes with raw comparisons or with `BabyBehave::Matchers::Expect(...).ToXxx(...)` (see [Fluent matchers](#fluent-matchers) below) exactly as well as with anything else — `SoftCheck` only adds naming/grouping on top.

## Fluent matchers

[`include/BabyBehave/matchers.hpp`](include/BabyBehave/matchers.hpp) is a small, standalone, dependency-free helper for more descriptive failure messages inside a step body. It has no include on `bdd.hpp` and no knowledge of `TestContext`/`BabyBehaveTest`, so it's just as usable outside BabyBehave entirely — it's a separate header (like `reporters.hpp` below) so consumers who don't want it never pay for it.

```cpp
#include <BabyBehave/matchers.hpp>
using namespace BabyBehave::Matchers;

bool AlarmWillRing(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    return Expect(alarmClock->GetHour()).ToEqual(7);
}
```

`Expect(value)` returns an `Expectation<T>` with:

- `ToEqual(expected)` / `ToNotEqual(expected)`
- `ToBeTrue()` / `ToBeFalse()`
- `ToBeGreaterThan(expected)` / `ToBeGreaterOrEqualTo(expected)`
- `ToBeLessThan(expected)` / `ToBeLessOrEqualTo(expected)`
- `ToContain(item)` — substring search for string-like types (`std::string`, `std::string_view`, `const char*`), element search (via `std::find`) for anything usable with `std::begin`/`std::end`
- `ToBeNull()` / `ToNotBeNull()` — works on raw/smart pointers and anything nullptr-comparable, and on optional-like types (anything with a `.has_value()` member)

Every `ToXxx()` returns `bool`, so it can be used directly as a step's `return ...;`; on failure it prints a descriptive `"Expect(...) failed: expected <actual> to <verb> <expected>"` message to `std::cerr` before returning `false` (values that aren't stream-insertable print as `"(non-printable value)"` instead of failing to compile). See [`examples/Matchers.cpp`](examples/Matchers.cpp) for a complete worked example.

## Structured output: JUnit XML / TAP

[`include/BabyBehave/reporters.hpp`](include/BabyBehave/reporters.hpp) turns a collected `TestResult`/`StepResult` (see [Collecting results instead of exiting](#collecting-results-instead-of-exiting)) into two CI-friendly formats:

```cpp
#include <BabyBehave/reporters.hpp>

std::vector<TestResult> results = { result1, result2, result3 };
std::cout << BabyBehave::BDD::Reporters::ToJUnitXml(results, "BabyBehave.SelfTest") << '\n';
std::cout << BabyBehave::BDD::Reporters::ToTap(results) << '\n';
```

- `ToJUnitXml(results, suiteName = "BabyBehave")` — a single `<testsuite>` with one `<testcase classname="{testName}" name="{stepLabel}: {stepName}">` per `StepResult`; a failed one gets a nested `<failure message="...">`, and a captured source location is split into `file`/`line` `<testcase>` attributes. Understood by GitHub Actions test-report actions, GitLab's JUnit report artifact type, Jenkins' JUnit plugin, and most other CI dashboards.
- `ToTap(results)` — Test Anything Protocol output: a `1..N` plan line followed by one `ok`/`not ok` line per `StepResult`, with a `# message` diagnostic line under failing ones. Consumable by `prove` and other generic TAP harnesses.

Both also have single-`TestResult` convenience overloads, and both are pure: they format and return a `std::string`, with no file I/O of their own — it's up to the caller to print it or write it wherever CI expects it.

Like `matchers.hpp`, this lives in its own header rather than in `bdd.hpp` (it `#include`s `"bdd.hpp"` itself, since it exists specifically to format `TestResult`/`StepResult`), so consumers who don't want it don't pay for it. It only makes sense for scenarios run under `SetCollectFailuresMode(true)`: in the default mode a failed step invokes the (by default `std::exit`-ing) failure callbacks before `Execute()` ever returns, so there is no complete `TestResult` to serialize in that case. Only feed it `TestResult`s from `SetCollectFailuresMode(true)` scenarios (a scenario that passed entirely still produces a valid, empty-but-meaningful `TestResult`). See [`tests/bdd/test_SelfTest.cpp`](tests/bdd/test_SelfTest.cpp), which accumulates results from its collect-failures-mode scenarios and writes both `selftest-results.xml` and `selftest-results.tap` at the end of `main()`.

## Gherkin support (runtime interpreter, on by default)

BabyBehave includes a runtime `.feature` file interpreter for teams that prefer Gherkin's structured syntax alongside BabyBehave's fluent C++ DSL. It is **on by default** but can be disabled via `BABYBEHAVE_DISABLE_GHERKIN` (see [below](#opting-out-of-gherkin) for when you might want to).

### Basic usage

```cpp
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

int main() {
    Gherkin::StepRegistry registry;
    
    registry.RegisterGiven("an empty basket", [](TestContext& ctx) {
        ctx.Set("basket", std::make_shared<Basket>());
        return true;
    });
    registry.RegisterWhen("I add {int} apples", [](TestContext& ctx, int count) {
        ctx.Get<std::shared_ptr<Basket>>("basket")->Add("apple", count);
        return true;
    });
    registry.RegisterThen("the basket contains {int} items", [](TestContext& ctx, int expected) {
        return ctx.Get<std::shared_ptr<Basket>>("basket")->Count() == expected;
    });
    
    const auto feature = R"gherkin(
        Feature: Shopping basket
        
        Scenario: Adding an item increases the count
            Given an empty basket
            When I add 3 apples
            Then the basket contains 3 items
    )gherkin";
    
    Gherkin::RunFeature(feature, registry, "Shopping basket");
}
```

The interpreter supports:
- **Feature labels and Scenarios** — organized test flows with readable names
- **Background steps** — shared preconditions for multiple scenarios
- **Step parameters** — `{int}`, `{float}`, `{string}`, `{word}` placeholders with automatic type conversion
- **Tags** — `@tag` annotations for scenario filtering and hook registration
- **Before/After hooks** — tag-scoped setup/teardown via `AddBeforeHook()`/`AddAfterHook()`
- **Comments** — `# comments` in `.feature` files are parsed and ignored

`RunFeature()` also takes an optional fourth `onFailure` parameter (`Gherkin::GherkinFailureCallback`, i.e. `std::function<void(std::string_view)>`) for redirecting Gherkin-sourced failures (a parse error or a failing Scenario) to your own handler instead of the library's default print-and-`exit(EXIT_FAILURE)` behavior:

```cpp
FeatureResult RunFeature(std::string_view featureText, StepRegistry& registry,
                          std::string_view featureLabel = "<feature>",
                          const GherkinFailureCallback& onFailure = impl::DefaultGherkinFailureAction);
```

A callback that returns normally instead of exiting/throwing lets `RunFeature()` keep going across the whole Feature and return a `FeatureResult` with `allPassed=false` for you to inspect — see [`GherkinCustomFailureHandler.cpp`](examples/gherkin/GherkinCustomFailureHandler.cpp) below.

### Examples

Three core Gherkin examples live directly in [`examples/`](examples/); the rest (including two multi-file registry-reuse demos) live in [`examples/gherkin/`](examples/gherkin/):

- **[`GherkinBasket.cpp`](examples/GherkinBasket.cpp)** — basic Given/When/Then and step-parameter placeholders
- **[`GherkinBackground.cpp`](examples/GherkinBackground.cpp)** — shared Background steps across multiple scenarios
- **[`GherkinTagsAndHooks.cpp`](examples/GherkinTagsAndHooks.cpp)** — tag-scoped `@tag` filters and Before/After hook registration
- **[`gherkin/GherkinUnmatchedStep.cpp`](examples/gherkin/GherkinUnmatchedStep.cpp)** — demonstrating fail-hard behavior on unmatched steps
- **[`gherkin/GherkinCollectFailures.cpp`](examples/gherkin/GherkinCollectFailures.cpp)** — forced collect-failures mode to gather all step outcomes
- **[`gherkin/GherkinPlaceholders.cpp`](examples/gherkin/GherkinPlaceholders.cpp)** — all four placeholder types (`{int}`, `{float}`, `{string}`, `{word}`)
- **[`gherkin/GherkinMultiThreaded.cpp`](examples/gherkin/GherkinMultiThreaded.cpp)** — concurrent `RunFeature()` calls per thread with independent registries
- **[`gherkin/GherkinAdvanced.cpp`](examples/gherkin/GherkinAdvanced.cpp)** — combined realistic feature with multiple scenarios
- **[`gherkin/GherkinVeryAdvanced.cpp`](examples/gherkin/GherkinVeryAdvanced.cpp)** — multi-feature scenarios with integration of `reporters.hpp`
- **[`gherkin/GherkinCustomFailureHandler.cpp`](examples/gherkin/GherkinCustomFailureHandler.cpp)** — a custom `onFailure` callback that collects failure messages instead of exiting

**Registry reuse (`StepRegistry::Merge()`)** — [`gherkin/BakerySteps.hpp`](examples/gherkin/BakerySteps.hpp) and [`gherkin/LibrarySteps.hpp`](examples/gherkin/LibrarySteps.hpp) are shared step-definition libraries, each reused unmodified across three example files with genuinely different scenarios (reading their `.feature` text from real files under [`gherkin/features/`](examples/gherkin/features/), not embedded strings):

- **[`gherkin/GherkinBakeryStandardOrder.cpp`](examples/gherkin/GherkinBakeryStandardOrder.cpp)** — standard cake order paid in full
- **[`gherkin/GherkinBakeryAllergenSubstitution.cpp`](examples/gherkin/GherkinBakeryAllergenSubstitution.cpp)** — allergen substitution surcharge, plus `Merge()` for a file-specific step
- **[`gherkin/GherkinBakeryLateCancellation.cpp`](examples/gherkin/GherkinBakeryLateCancellation.cpp)** — late cancellation forfeits the deposit (intentionally exits non-zero)
- **[`gherkin/GherkinLibraryStandardLending.cpp`](examples/gherkin/GherkinLibraryStandardLending.cpp)** — checkout and on-time return
- **[`gherkin/GherkinLibraryHoldsAndReservations.cpp`](examples/gherkin/GherkinLibraryHoldsAndReservations.cpp)** — hold queue fulfillment, plus `Merge()` for a file-specific step
- **[`gherkin/GherkinLibraryOverdueFines.cpp`](examples/gherkin/GherkinLibraryOverdueFines.cpp)** — overdue fine calculation
- **[`gherkin/GherkinLibraryConcurrentLending.cpp`](examples/gherkin/GherkinLibraryConcurrentLending.cpp)** — one shared `StepRegistry`, built once, fanned out across four threads each running `RunFeature()` against a different branch's `.feature` file concurrently

### Opting out of Gherkin

Gherkin support requires C++20 (specifically `<concepts>` and `<optional>`), while the rest of BabyBehave gracefully falls back to C++17. If you're targeting pure C++17 or simply don't need Gherkin, define `BABYBEHAVE_DISABLE_GHERKIN` **before** including the header:

```cpp
#define BABYBEHAVE_DISABLE_GHERKIN
#include <BabyBehave/bdd.hpp>

// BabyBehave::BDD::Gherkin is NOT defined; everything else works normally.
// The C++20 includes are not pulled in either, so no compilation penalty.
using namespace BabyBehave::BDD;

// ... write tests using the fluent API as usual
```

### Full design rationale

For design decisions, feature coverage rationale, and why only AND/subset tag matching in v0.8.0, see [`docs/design/gherkin-support.md`](docs/design/gherkin-support.md).

## Configuration reference

This library recognizes several compile-time `#define`s and runtime environment variables, including `BABYBEHAVE_DISABLE_GHERKIN`, `BABYBEHAVE_NO_SHORT_MACROS`, `BABYBEHAVE_QUIET` (env var), and `BABYBEHAVE_STYLE` (env var). For a comprehensive reference with examples, see [`docs/configuration.md`](docs/configuration.md).

## Running scenarios concurrently

Each `BabyBehaveTest` (created by `Given`/`GivenA`) owns its `TestContext` as a private member — it's never shared between scenarios unless you go out of your way to pass one `TestContext&` into several of them. That means launching independent scenarios one-per-thread (e.g. via `std::async`) needs no locking at all: there is nothing for the threads to race on. `TestContext` itself is **not** thread-safe (it's backed by a plain `std::unordered_map` with no internal synchronization), so the one rule is: don't share a single `TestContext` across threads. See [`examples/MultiThreaded.cpp`](examples/MultiThreaded.cpp) for a complete, working example — including an `#if 0`-guarded, never-compiled sketch of the unsafe shared-`TestContext` pattern it deliberately avoids.

## Installation

### Option A: FetchContent / add_subdirectory (vendoring)

```cmake
include(FetchContent)
FetchContent_Declare(
  BabyBehave
  GIT_REPOSITORY https://github.com/crsnplusplus/BabyBehave.git
  GIT_TAG main
)
FetchContent_MakeAvailable(BabyBehave)

add_executable(your_tests your_tests.cpp)
target_link_libraries(your_tests PRIVATE BabyBehave::BabyBehave)
```

The same works with a plain `add_subdirectory(BabyBehave)` if you vendor the source directly instead of fetching it.

### Option B: find_package (installed package)

BabyBehave ships a CMake package config, so it can be installed once and consumed from anywhere:

```bash
git clone https://github.com/crsnplusplus/BabyBehave.git
cd BabyBehave
cmake -B build -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build
cmake --install build
```

```cmake
find_package(BabyBehave REQUIRED)

add_executable(your_tests your_tests.cpp)
target_link_libraries(your_tests PRIVATE BabyBehave::BabyBehave)
```

Either way, linking against the `BabyBehave::BabyBehave` target is all you need — it's an `INTERFACE` target that only adds the include path (and requires C++23, see [Requirements](#requirements) below).

### Option C: vcpkg (overlay port)

BabyBehave isn't in the curated vcpkg registry yet, but the repo ships an overlay port at `ports/babybehave/`. Until a tagged release exists, install straight from `main` with `--head`:

```bash
git clone https://github.com/crsnplusplus/BabyBehave.git
vcpkg install babybehave --head --overlay-ports=./BabyBehave/ports/babybehave
```

```cmake
find_package(BabyBehave REQUIRED)
target_link_libraries(your_tests PRIVATE BabyBehave::BabyBehave)
```

### Option D: Conan

BabyBehave ships a Conan 2.x recipe (`conanfile.py`) for a header-only `header-library` package:

```bash
git clone https://github.com/crsnplusplus/BabyBehave.git
cd BabyBehave
conan create . --version 0.7.19
```

Then, from a consumer's `conanfile.txt`:

```ini
[requires]
babybehave/0.7.19

[generators]
CMakeDeps
CMakeToolchain
```

```cmake
find_package(BabyBehave REQUIRED)
target_link_libraries(your_tests PRIVATE BabyBehave::BabyBehave)
```

### Option E: Bazel (Bzlmod)

Not yet in the Bazel Central Registry, so depend on the repo directly via `git_override` (or `local_path_override` for a vendored copy) in your `MODULE.bazel`:

```python
bazel_dep(name = "babybehave", version = "0.7.19")
git_override(
    module_name = "babybehave",
    remote = "https://github.com/crsnplusplus/BabyBehave.git",
    commit = "<commit-sha>",  # or a v0.7.19 tag once one exists
)
```

```python
# your BUILD.bazel
cc_test(
    name = "your_tests",
    srcs = ["your_tests.cpp"],
    deps = ["@babybehave"],
)
```

Bazel doesn't infer a project-wide C++ standard the way CMake does, so add `--cxxopt=-std=c++23` (or `-std=c++17`, given the fallback story below) to your own `.bazelrc`.

### Option F: manual copy

BabyBehave is genuinely header-only and has zero third-party dependencies, so the simplest possible integration is copying `include/BabyBehave/*.hpp` straight into your project and adding that directory to your compiler's include path — no build-system integration at all.

## C++ standard support

Most of `bdd.hpp` (fluent API, matchers, reporters) is C++17 compatible, with graceful C++23 enhancements where available. **The Gherkin block specifically requires C++20** (`<concepts>`, `<optional>`, modern regex semantics). Consumers targeting pure C++17 can disable Gherkin via `BABYBEHAVE_DISABLE_GHERKIN` (see [Opting out of Gherkin](#opting-out-of-gherkin) above).

For the non-Gherkin parts, the header checks the relevant `<version>` feature-test macros and falls back when a C++23 facility isn't available:

| Facility | C++23 | C++17 fallback | Guarded by |
| --- | --- | --- | --- |
| Step/context-setup callable storage | `std::move_only_function` | `std::function` | `__cpp_lib_move_only_function` |
| Console output | `std::println` | `std::cout << ... << '\n'` | `__cpp_lib_print` |
| Step/`Given` call-site capture (see [Call-site diagnostics](#call-site-diagnostics)) | `std::source_location` | unavailable — `StepResult::location` stays empty, no message suffix | `__cpp_lib_source_location` |

So most of the header will still compile under `-std=c++17` (except the Gherkin block). **Note:** the `BabyBehave::BabyBehave` CMake target declares `cxx_std_23` as a compile feature requirement (`src/CMakeLists.txt`), so consumers who link against that target are bumped to C++23 by CMake regardless. If you need to build under C++17, vendor/copy the header directly instead of linking the CMake target (and optionally define `BABYBEHAVE_DISABLE_GHERKIN` if you want to be extra conservative).

## CMake options

Two opt-in options are available when configuring the project itself (not needed by consumers just linking `BabyBehave::BabyBehave`):

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake -B build -DBABYBEHAVE_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# gcov-based code coverage (also builds an HTML `coverage-report` target if lcov + genhtml are found)
cmake -B build -DBABYBEHAVE_ENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target coverage-report   # only if lcov/genhtml are installed
```

(Gherkin is now on by default; there is no CMake option to gate it, since disabling it is a consumer's compile-time decision via `BABYBEHAVE_DISABLE_GHERKIN`.)

With `BABYBEHAVE_ENABLE_COVERAGE=ON` and `gcov` available, two independent coverage measurements are also available as build targets:

```bash
cmake --build build --target coverage-ut coverage-bbh
```

- **`coverage-ut`** — coverage of `bdd.hpp` as exercised by the gtest unit test suite in [`tests/`](tests/).
- **`coverage-bbh`** — coverage of `bdd.hpp` as exercised by the self-hosted [`tests/bdd/test_SelfTest.cpp`](tests/bdd/test_SelfTest.cpp) dogfood tests.

There are two separate targets, not one, because `bdd.hpp` is header-only and template-heavy: every binary that includes it compiles and instruments its own private copy of its inline code, with its own `.gcno`/`.gcda` pair. Keeping the two measurements' object directories separate keeps the reports naturally isolated from each other.

## Building the examples and tests

```bash
git clone https://github.com/crsnplusplus/BabyBehave.git
cd BabyBehave
cmake -B build
cmake --build build
ctest --test-dir build
```

See the [`examples/`](examples/) and [`tests/`](tests/) directories for working scenarios.

## Requirements

- To build this project's own CMake targets (examples, tests, and anything linking `BabyBehave::BabyBehave`): a C++23 compiler
- To vendor/include `bdd.hpp` directly, outside of this project's CMake:
  - **Default (with Gherkin enabled):** a C++20 compiler is required (for `<concepts>` and `<optional>`)
  - **Pure C++17:** possible by defining `BABYBEHAVE_DISABLE_GHERKIN` before including (see [C++ standard support](#c-standard-support) and [Opting out of Gherkin](#opting-out-of-gherkin))
- CMake 3.20+ only if you want to build the examples/tests, install the package, or use `find_package`; consuming the header directly requires nothing

## License

[MIT](LICENSE) — © 2023 Cristian Marletta
