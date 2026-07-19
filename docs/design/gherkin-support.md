# Gherkin support — shipped as opt-out in v0.8.0+

**Status:** shipped (v0.8.0+). Branch: `feature/gherkin-bdd-support`.
**Implementation:** Opt-out by default (see [Implementation: opt-out design](#implementation-opt-out-design) below). The one item that was deferred to v0.8.1 (the `onFailureCallback` extension point) has since shipped — see [v0.8.1: RunFeature's onFailureCallback extension point](#v081-runfeatures-onfailurecallback-extension-point).

## Implementation: opt-out design

This feature ships with Gherkin support **on by default** as an opt-out rather than opt-in. The rationale: during implementation, we discovered the Gherkin block's C++20 floor (it requires `<concepts>`, `<optional>`, and modern regex semantics unavailable in C++17). This trades "a consumer targeting pure C++17 must remember to define `BABYBEHAVE_DISABLE_GHERKIN`" for "everyone else gets Gherkin for free with zero opt-in ceremony." This was the project owner's explicit, informed choice.

To opt out (e.g., for a pure-C++17 build), `#define BABYBEHAVE_DISABLE_GHERKIN` **before** `#include <BabyBehave/bdd.hpp>`. When disabled: zero symbols, zero template instantiations, not even the extra `#include <concepts>`/`<optional>`/etc., so the consumer pays zero compile-time cost.

The original design proposal below (written before this decision) referred to opt-in behavior — see [Hard constraints (from the project owner)](#hard-constraints-from-the-project-owner) for the legacy wording.

## Important: API method naming (`RegisterGiven` vs. `Given`)

The `StepRegistry` methods are named `RegisterGiven`, `RegisterWhen`, `RegisterThen`, `RegisterAnd`, and `RegisterBut` — **not** the bare `Given`/`When`/etc. keyword names — because the latter collide with BabyBehave's own global fluent-DSL macros (`#define Given`, `#define When`, etc., used in the traditional C++ BDD interface). The `Register*` names avoid the collision and make it explicit that you are registering step definitions in a step registry, not invoking the fluent API directly.

## Hard constraints (from the project owner)

- Single `#include <BabyBehave/bdd.hpp>` must be enough — no separate `gherkin.hpp` (unlike the existing `matchers.hpp`/`reporters.hpp` precedent), and no offline code-generation tool.
- Zero third-party dependencies (no external Gherkin parser, no Boost, no regex library beyond `<regex>`/hand-rolled matching if needed).
- Zero cost when unused: everything lives behind a compile-time gate (originally designed as opt-in `#if defined(BABYBEHAVE_ENABLE_GHERKIN)`, shipped as opt-out `#if !defined(BABYBEHAVE_DISABLE_GHERKIN)` per [Implementation: opt-out design](#implementation-opt-out-design) above).
- Very modern C++ (C++23-first, same graceful C++17-fallback story as the rest of the header, using the existing `__cpp_lib_*` feature-test-macro pattern) — **with C++20 as the floor for the Gherkin block specifically**, discovered during implementation.

## Chosen approach: runtime interpreter, not a code generator

Two options were weighed:

1. **Runtime interpreter** (chosen) — `bdd.hpp` gains a small Gherkin parser that, at run time, reads `.feature` text (`std::string_view`, no file I/O of its own) and dispatches each step to a step-definition the consumer registered, driving the *existing* `BabyBehaveTest`/`TestContext`/`AddStep<StepType>` machinery. No build step, no generated files.
2. **Offline code generator** — a separate tool parses `.feature` files and emits `.cpp`. Rejected for v0.8.0: it needs a companion tool/build step outside the header, conflicting with "one hpp include" and the zero-codegen pitch. A standalone header `gherkin.hpp` would in fact be the *technically* cleaner home for this (zero cost even to read, consistent with the `reporters.hpp` precedent of including `bdd.hpp` from outside) — but it doesn't satisfy the firm "one include" requirement, so it was consciously set aside in favor of embedding directly in `bdd.hpp`.

## Gherkin-spec feature coverage for v0.8.0

Of the Gherkin grammar's discrete syntax elements, v0.8.0 covers **6 of 11 (~55%)**:

| # | Gherkin feature | v0.8.0? | Why |
|---|---|---|---|
| 1 | `Feature:` | ✅ | Doc-only grouping, near-zero cost. |
| 2 | `Background:` | ✅ | Requested explicitly. |
| 3 | `Scenario:`/`Example:` | ✅ | One `BabyBehaveTest` per Scenario, synthetic empty `ContextSetup`. |
| 4 | `Given/When/Then/And/But` steps | ✅ | Direct 1:1 mapping onto `AddStep<Precondition/Action/Postcondition/And/But>`. |
| 5 | Tags (`@smoke`) | ✅ | Requested explicitly; tag-scoped hooks depend on it. |
| 6 | Comments (`# ...`) | ✅ | Nearly free to skip while parsing; without it, any real-world `.feature` file with comments fails to parse. |
| 7 | `Rule:` | ❌ deferred | Adds a nesting level (tag/Background inheritance through Rule too) with no stated need yet. |
| 8 | Scenario Outline/Examples | ❌ deferred | Structurally the most expensive feature (real templating/row-expansion); not needed to clear the coverage bar. |
| 9 | Doc Strings | ❌ deferred | Needs a side-channel argument beyond `bool(TestContext&)`; doable later via `TestContext`, not free. |
| 10 | Data Tables | ❌ deferred | Same side-channel need as Doc Strings. |
| 11 | Spoken-language/i18n keywords | ❌ deferred | Disproportionate maintenance cost (dozens of languages) for a header-only, macro-gated library. |

Note: **Before/After are not Gherkin grammar** — they're Cucumber's glue-code/runtime mechanism (registered against tag filters), so they live on the `StepRegistry`, not in the `.feature` parser, and aren't counted in the table above. Step-parameter placeholders (`{int}`, `{float}`, `{string}`, `{word}`) are likewise part of *step-definition matching*, not `.feature` grammar — they fall under row 4.

## Architecture

- New `namespace BabyBehave::BDD::Gherkin { ... }` inside `bdd.hpp`, gated by `#if !defined(BABYBEHAVE_DISABLE_GHERKIN)` (opt-out; on by default). Defined (the default): full Gherkin interpreter. Disabled via `#define BABYBEHAVE_DISABLE_GHERKIN` before including: zero symbols, zero template instantiations, zero compile-time cost, not even the C++20-specific `#include`s — the only unavoidable residual cost is visual (a reader scrolls past the commented block in the header).
- **Explicit `Gherkin::StepRegistry`**, constructed and passed in by the consumer — no global auto-registration. This avoids hidden static state, `ThreadSanitizer`/static-init-order risk, and matches `bdd.hpp`'s existing style of always binding `BabyBehaveTest` to a named variable (see `bdd.hpp` around lines 887-899).
- **Unmatched step ⇒ fail hard**, consistent with the library's default exit-on-failure philosophy. `StepResult` stays a plain `bool passed` — no new "pending" state.
- Step-parameter syntax: cucumber-expression-lite placeholders (`{int}`, `{float}`, `{string}`, `{word}`), not raw regex.

### Mapping rules

| Gherkin | Maps to |
|---|---|
| `Feature:` | Doc-only grouping, no executable code. |
| `Scenario:` | One `BabyBehaveTest`, synthetic empty `ContextSetup` (`GivenAImpl(scenarioName, noop)`). |
| `Background:` steps | Same keyword→StepType mapping as Scenario steps, **prepended** to the Scenario's own `m_steps` — not folded into the synthetic `ContextSetup`, so per-step location/narration/`TestResult` attribution stays granular (a Background step failing must show up as its own `StepResult`, not collapse into one aggregate "ContextSetup failed"). |
| `Given` / `When` / `Then` | `AddStep<Precondition>` / `AddStep<Action>` / `AddStep<Postcondition>` |
| `And` / `But` | `AddStep<And>` / `AddStep<But>` directly (1:1, confirmed via `kAndConditionFailedMessage`/`kButConditionFailedMessage`) |
| Tags (`@tag`) | Accumulated per Scenario as the union of its own tags and its Feature's tags (Feature-level tags are inherited by every Scenario in it). |
| `Before`/`After` hooks | Registered on `StepRegistry` with a tag filter; adapted to `StepFunction` (`bool(TestContext&)`) via a trivial wrapper and injected as ordinary `Precondition`/`Postcondition` steps, prefixed in `stepName` (e.g. `"[Before] name"`) since `stepLabel` itself is fixed to the six existing `StepVariant` labels and isn't parametrizable without touching `bdd.hpp` outside this additive layer. |

Execution order per Scenario: **Before hooks (registration order) → Background → Scenario's own steps → After hooks (registration order)**. Tag matching for hooks/filters is **AND/subset only** in v0.8.0 (no OR/NOT) — OR is achievable by registering the same hook multiple times under single-tag filters (a consumer-side workaround, no new engine complexity); NOT is deferred.

## Examples

### 1 — plain scenario, typed placeholders

```gherkin
# examples/gherkin/basket.feature
Feature: Shopping basket

  Scenario: Adding an item increases the count
    Given an empty basket
    When I add 3 apples
    Then the basket contains 3 items
```

```cpp
BabyBehave::BDD::Gherkin::StepRegistry registry;
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
```

### 2 — Background shared across scenarios

```gherkin
Feature: Coffee machine

  Background:
    Given a freshly booted coffee machine
    And a full water tank

  Scenario: Brewing an espresso
    When I brew an espresso
    Then a cup is served

  Scenario: Brewing without beans fails
    Given no coffee beans
    When I brew an espresso
    Then the brew is rejected
```

Each Scenario gets the two Background steps prepended to its own `m_steps`; per-step narration/location/collect-failures behavior is unaffected.

### 3 — tags and tag-scoped Before/After hooks

```gherkin
Feature: Payments

  @slow @integration
  Scenario: Charging a card via the real gateway
    Given a valid card
    When I charge 10 EUR
    Then the charge succeeds
```

```cpp
registry.AddBeforeHook({"slow"}, [](TestContext& ctx) {
    ctx.Set("startTime", Clock::now());
});
registry.AddAfterHook({"slow"}, [](TestContext& ctx) {
    LogDuration(ctx.Get<TimePoint>("startTime"));
});
registry.AddBeforeHook({}, [](TestContext& ctx) {   // empty tag list = always runs
    ctx.Set("db", OpenTestDatabase());
});
```

A hook whose tag matches no scenario (e.g. a typo, `@smoke` vs `@somke`) is silent and expected — **not** an error, unlike an unmatched step. This asymmetry is intentional and must stay documented, since the fail-hard philosophy does not apply uniformly here.

### 4 — unmatched step: fail hard

```gherkin
Scenario: Typo in the step text
  Given an empty basket
  When I addd 3 apples     # no registered step definition matches
```

The interpreter fails immediately with a "no step definition matches: 'I addd 3 apples'" message, consistent with the library's exit-on-failure default.

### 5 — forced collect-failures mode and its consequence

```gherkin
Scenario: Multiple assertions in one flow
  Given a valid card
  When I charge -5 EUR      # fails
  Then the charge succeeds  # still runs, reported as failed
```

`BabyBehaveTest` only has two modes today: exit-on-first-failure (default) or run-everything-and-report (`SetCollectFailuresMode(true)`) — there is no third "stop after first failure but still run After hooks" mode. To guarantee After hooks always run (a core Before/After contract), the Gherkin interpreter must **force collect-failures mode internally** on every generated `BabyBehaveTest`, and reimplement fail-hard itself by inspecting `TestResult::allPassed` after `Execute()`. Consequence, explicitly accepted for v0.8.0: unlike real Cucumber (which marks subsequent steps "skipped" and does not execute them), BabyBehave's Gherkin mode **executes and reports every step**, even after an earlier failure. This must be documented as a known behavioral difference, not hidden.

## Two bugs to fix as part of this work, not after

1. **Double "Given" in output** — `BabyBehaveTest::Execute()` (bdd.hpp ~lines 900-911) unconditionally prints `"Given " + scenarioName` before the first real step; for a Gherkin scenario (synthetic empty `ContextSetup`) whose first real step is itself a `Given` (own or from Background), this doubles the prefix. Must be suppressed/adjusted specifically for the synthetic-setup case.
2. **`source_location` misattribution** — `AddStep`/`GivenAImpl` capture `std::source_location::current()` at their own C++ call site (~lines 753-781, 848-876) for failure attribution. Calling them from one internal interpreter dispatch point would make every Gherkin-sourced failure report the parser's location instead of the real `.feature` line/column. The interpreter must track line/column while parsing and inject a synthetic `source_location` per step, bypassing the automatic default-argument capture for this path.

## Line-count estimate

| Component | Lines |
|---|---|
| Base (Feature/Scenario/steps/placeholders, `StepRegistry` matching, synthetic-location injection, bug #1 fix) | 150–250 |
| Background (reuses step parsing, prepend to `m_steps`) | 25–35 |
| Tags (parsing, Feature→Scenario union, AND/subset predicate) | 30–40 |
| Before/After hooks (`StepRegistry` API, adapter, tag dispatch, `stepName` prefixing, external fail-hard loop) | 45–60 |
| **Total** | **~280–420** (+22–32% over current 1293 lines) |

For comparison, Scenario Outline/Examples alone (deferred) was estimated at 450–650 lines by itself — real templating/row-expansion is structurally more expensive than the prepend/filter operations Background+Tags+Hooks need on the existing flat step-list model.

## Trimming opportunities in current `bdd.hpp` (partial offset, not a wash)

- `executeStep()` (~lines 998-1096, six near-identical `if constexpr` branches) → a per-type metadata table (`template<typename T> struct StepMeta`), ~50 lines saved. **Caveat:** the current code has a comment (~lines 991-997) explicitly defending this repetition as "inherent, not accidental" complexity — read that comment before approving the refactor, since this recommendation intentionally contradicts it.
- The 14 fluent macros (`Given/GivenA/With/WithI/...`, ~lines 1276-1289): the `-I` variants are identical in body to their base — turning them into aliases (`#define GivenA(func) Given(func)`) removes a silent-divergence risk. Maintainability win, ~0 net lines.
- C++23/C++17 fallback pairs in `BabyBehaveTest` (~768-781), `AddStep<StepType>` (~860-876), `GivenAImpl` (~1256-1270): consolidatable behind a `CaptureLocationOrEmpty` helper, ~20-30 lines saved.

Realistic total: ~70-80 lines, a partial offset against the new feature's 280-420, not a net wash.

## Modern C++ techniques (verified against g++16.1.1 / clang++22.1.8)

| Technique | Verdict | C++17 fallback |
|---|---|---|
| `std::expected<T,E>` | Solid — `and_then`/`transform`/`or_else` chaining works. Guard: `__cpp_lib_expected`. | Custom `Result<T,E>` (does not exist yet, must be written) |
| Concepts | Solid on both compilers, readable error messages. Already C++20, within the existing minimum target — no dedicated guard needed. | `static_assert(std::is_invocable_r_v<bool, F, TestContext&>)` if needed pre-C++20 |
| `std::generator<string_view>` / coroutines for line-by-line parsing | Compiles, no dangling (ASan-checked) — **rejected anyway**: parsing a `.feature` file isn't a hot path, and the coroutine complexity plus a dedicated fallback-coverage test binary in CI isn't justified for this. | N/A — not used on either path |
| `ranges::views::split` / `lazy_split` | **Fails** on both compilers with `string_view` (needs `contiguous_iterator`, gets `forward_iterator`). | A manual `find`/`substr` loop is used on **both** C++23 and C++17 paths — simpler and more portable than fighting the range adaptor. |
| `consteval` step-pattern pre-validation | Works (two-stage consteval / NTTP fixed_string) but marginal benefit — patterns register once at startup (one-shot runtime cost), so the "compile-time" win is dubious against the NTTP boilerplate. Nice-to-have, not required for v0.8.0. | Ordinary runtime parsing, no correctness loss |

## Known design risks flagged for implementation

1. Tag-expression grammar is AND/subset-only in v0.8.0 — no boolean OR/NOT. Document as an explicit restriction.
2. Tag inheritance (Feature → Scenario) must be computed once per Feature and unioned into every Scenario before any predicate evaluation — checking only local Scenario tags would silently break Feature-level tag filtering.
3. Hook execution order for multiple matching hooks: **registration order** for both Before and After (not reversed/stack-unwinding for After — a natural but wrong assumption; real Cucumber runs After hooks in registration order too).
4. A hook matching no scenario is not an error (contrast with unmatched steps, which are fail-hard) — this asymmetry is intentional, not a gap.
5. Step-definition matching (pattern+placeholder) and hook tag-matching (pure subset-check) are structurally different algorithms — keep them as separate internal matchers, don't prematurely unify under one generic "matcher" abstraction.
6. Execution order between Before hooks and Background: **Before runs first** (Before hooks prepare state that Background steps may read) — the reverse order is equally plausible at first glance, so this must be pinned down explicitly rather than left to assumption.
7. Forcing collect-failures mode internally for every Gherkin scenario silently bypasses `SetOnConditionNotVerifiedCallback`/`SetOnExceptionCallback` for anything coming through the Gherkin path — see [Deferred to v0.8.1](#deferred-to-v081).

## v0.8.1: RunFeature's onFailureCallback extension point

Ships the item that was previously deferred (see risk #7 in [Known design risks flagged for implementation](#known-design-risks-flagged-for-implementation) above): `RunFeature()` gains a fourth, optional parameter letting an advanced consumer redirect Gherkin-sourced failures to their own test harness instead of the internal default-exit fail-hard.

```cpp
using GherkinFailureCallback = std::function<void(std::string_view)>;

FeatureResult RunFeature(std::string_view featureText, StepRegistry& registry,
                          std::string_view featureLabel = "<feature>",
                          const GherkinFailureCallback& onFailure = impl::DefaultGherkinFailureAction);
```

- **Plain `std::function`, not `move_only_function`** — `onFailure` can be invoked more than once per `RunFeature()` call: once for a parse error, or once per failing Scenario across the whole Feature.
- **Default behavior is unchanged** — `impl::DefaultGherkinFailureAction` prints the message to stderr and calls `std::exit(EXIT_FAILURE)`, byte-identical to the pre-v0.8.1 fail-hard path (the existing `EXPECT_EXIT` death tests in `tests/test_Gherkin_Integration.cpp` cover this and needed no changes).
- **Parse-failure path**: instead of printing and exiting directly, `RunFeature()` now calls `onFailure(parsed.error)` and, if `onFailure` returns normally instead of exiting/throwing, returns a `FeatureResult` with an empty `featureName`, no `scenarioResults`, and `allPassed=false` — `RunFeature()` itself never exits/throws here, leaving that decision entirely to the callback.
- **Scenario-failure path**: `impl::ReportScenarioFailureAndExit()` (the old `[[noreturn]]` print-and-exit function) was removed and replaced by `impl::FormatScenarioFailureMessage()`, which builds the identical diagnostic text but *returns* it as a `std::string` instead of printing/exiting. `RunScenario()` calls `onFailure(FormatScenarioFailureMessage(result))`; since collect-failures mode is already forced internally (see [forced collect-failures mode](#5--forced-collect-failures-mode-and-its-consequence) above), execution simply continues and `RunScenario()` returns `result` regardless of what `onFailure` does. If `onFailure` exits/throws (the default), execution stops there, identical to today. If it returns normally, `RunFeature()` moves on to the next Scenario — a "collect Gherkin failures across the whole Feature" mode that falls out naturally from a non-exiting callback.
- `FeatureResult`'s doc comment was updated accordingly: it's only true that `RunFeature()` "never returns with `allPassed==false`" for the *default* callback; a non-exiting custom callback is exactly what makes a `false` return possible.
- See [`examples/GherkinCustomFailureHandler.cpp`](../../examples/GherkinCustomFailureHandler.cpp) for a full example that collects every failure message into a `std::vector<std::string>` instead of exiting, then decides its own process exit code from `FeatureResult::allPassed` at the end.

## Backlog beyond v0.8.1

- Everything in the "❌ deferred" rows of the [feature coverage table](#gherkin-spec-feature-coverage-for-v080) (`Rule:`, Scenario Outline/Examples, Doc Strings, Data Tables, i18n keywords) remains open backlog, to be scoped when there's a concrete need.
- README wording follow-up ("no Gherkin files to parse" → qualify for the opt-in default), small documentation-only task, not yet assigned.
