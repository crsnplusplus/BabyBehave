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

## Gherkin feature coverage (shipped features)

Of the Gherkin grammar's discrete syntax elements, current coverage is **9 of 11 (~82%)**:

| # | Gherkin feature | v0.8.0? | Why |
|---|---|---|---|
| 1 | `Feature:` | ✅ | Doc-only grouping, near-zero cost. |
| 2 | `Background:` | ✅ | Requested explicitly. |
| 3 | `Scenario:`/`Example:` | ✅ | One `BabyBehaveTest` per Scenario, synthetic empty `ContextSetup`. |
| 4 | `Given/When/Then/And/But` steps | ✅ | Direct 1:1 mapping onto `AddStep<Precondition/Action/Postcondition/And/But>`. |
| 5 | Tags (`@smoke`) | ✅ | Requested explicitly; tag-scoped hooks depend on it. |
| 6 | Comments (`# ...`) | ✅ | Nearly free to skip while parsing; without it, any real-world `.feature` file with comments fails to parse. |
| 7 | `Rule:` | ❌ deferred | Adds a nesting level (tag/Background inheritance through Rule too) with no stated need yet. |
| 8 | Scenario Outline/Examples | ✅ | Expanded at parse time into one independent Scenario per data row (v0.9.0+). |
| 9 | Data Tables | ✅ | Tabular arguments to steps with header-aware cell lookups, opt-in via trailing `const DataTable&` parameter (v0.9.0+). |
| 10 | Doc Strings | ✅ | `"""`-delimited multi-line string arguments to steps, opt-in via trailing `const std::string&` parameter; reuses the same `RawArgument` plumbing as Data Tables (v0.9.0+). |
| 11 | Spoken-language/i18n keywords | ❌ deferred | Disproportionate maintenance cost (dozens of languages) for a header-only, macro-gated library. |

Note: **Before/After are not Gherkin grammar** — they're Cucumber's glue-code/runtime mechanism (registered against tag filters), so they live on the `StepRegistry`, not in the `.feature` parser, and aren't counted in the table above. Step-parameter placeholders (`{int}`, `{float}`, `{string}`, `{word}`) are likewise part of *step-definition matching*, not `.feature` grammar — they fall under row 4.

## Runtime/API extensions (not Gherkin grammar)

Some shipped (and planned) features are **not** new Gherkin-spec grammar constructs — they reuse existing grammar (typically the ordinary `@tag` syntax) and add runtime/API behavior on top. Counting them in the [grammar coverage table](#gherkin-feature-coverage-shipped-features) above would inflate that denominator with things that aren't discrete *syntax elements* of the Gherkin language itself, so they're tracked separately here instead:

| # | Extension | Shipped? | Why it's not grammar |
|---|---|---|---|
| 1 | Timeout annotations (`@timeout:<value><unit>`) | ✅ (v0.9.0 Phase 1) | Reuses the existing `@tag` syntax verbatim (`AppendTagsFromLine` needs no changes); `@timeout:5s` is parsed as an ordinary tag string and interpreted only when computing a Scenario's execution policy. No new token, keyword, or line shape is added to the `.feature` grammar. |
| 2 | Parallel scenario execution (`enableParallelScenarios` parameter) | ✅ (v0.9.0 Phase 1) | A `RunFeature()` parameter, not new Gherkin grammar — when `true`, all scenarios dispatch concurrently via `std::async`, but result ordering stays deterministic via pre-allocated index slots. |
| 3 | Retry/flaky annotations (`@retry:N`) | ✅ (v0.9.0 Phase 1) | Reuses the existing `@tag` syntax verbatim; `@retry:3` is parsed as an ordinary tag string and interpreted only when computing a Scenario's execution policy (its total-attempt count). No new token, keyword, or line shape is added to the `.feature` grammar. |
| 4 | Tag expressions (AND/OR/NOT) | ✅ (v0.9.0 Phase 1) | A pure C++ runtime/API extension: `StepRegistry` gains two new registration methods (`AddBeforeHookExpr`/`AddAfterHookExpr`) that accept a boolean tag-matching expression string instead of a static tag vector. The `.feature` syntax is unchanged — expressions are C++ strings passed at registration time, not `.feature`-file syntax. |
| 5 | Suite-level Before/After-all hooks | ✅ (v0.9.0 Phase 1) | A pure C++ registration-time concept: `StepRegistry` gains two new methods (`AddBeforeAllHook`/`AddAfterAllHook`) that register suite-wide hooks running exactly once per Feature (Before-ALL before any Scenario, After-ALL after all Scenarios). No `.feature` grammar changes; hooks are unconditional (no tag filtering) and take no arguments. |

Non-grammar runtime/API extensions in this vein belong in this table, not in the grammar-coverage table above — reserve that one for constructs that are actually part of the Gherkin/Cucumber grammar spec (like `Scenario Outline`/`Examples`, `Data Tables`, or `Doc Strings`).

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


### 6 — Scenario Outline / Examples: data-driven scenario expansion

```gherkin
Feature: Customer loyalty discounts

  Scenario Outline: Loyalty tier determines final price
    Given a customer with <tier> status
    When they purchase items totaling <order_total> EUR
    Then they receive a <discount>% discount
    And the final price is <final_price> EUR

    Examples:
      | tier     | order_total | discount | final_price |
      | bronze   | 100         | 5        | 95          |
      | silver   | 100         | 10       | 90          |
      | gold     | 100         | 15       | 85          |
```

A `Scenario Outline:` (synonym `Scenario Template:`) is a template for multiple related scenarios, each driven by one row of data from an `Examples:` block (synonym `Scenarios:`). The Examples block is a pipe-delimited table: the first row defines column headers (matching the `<placeholder>` names in the Outline's step text), and each subsequent row becomes one concrete scenario.

**Expansion and naming:** At parse time, each data row is substituted into the Outline's template, creating one independent `BabyBehaveTest` per row. For each expanded scenario, `<name>` tokens in the step text are replaced textually *before* cucumber-expression-style placeholders (`{int}`, `{float}`, etc.) are matched. This two-stage substitution means they compose naturally: a step like `"the customer with <tier> status has <discount>% off"` becomes `"the customer with gold status has 15% off"` after data substitution, and then a registered step pattern like `"the customer with {word} status has {int}% off"` matches it with arguments `("gold", 15)`.

In `FeatureResult::scenarioResults`, each expanded scenario's name is **`"<original name> (Examples row N)"`** where N is the 1-based index among **data rows only** (the header row is not counted in the sequence): the first data row produces "Examples row 1", the second produces "Examples row 2", and so on.

**Important: Data Tables and Doc Strings are NOT placeholder-substituted during expansion.** When a `Scenario Outline` has steps with Data Tables or Doc Strings, those tabular/multiline arguments are copied verbatim to every expanded scenario — `<placeholder>` tokens in the outline's step text are substituted (as described above), but any Data Table or Doc String attached to that step is left unchanged and repeated in full for each row. This diverges from some Gherkin implementations (e.g. some Cucumber variants) where table content can contain placeholders. BabyBehave's behavior is: substitute placeholders in step text, copy Data Tables/Doc Strings as-is. This is intentional and tested.

**Parse errors (all fail hard):**
- An Outline with no `Examples:` block following it is a parse error.
- An `Examples:` block with zero data rows (only a header) is a parse error.
- A cell-count mismatch (header columns ≠ any data row's cell count) is a parse error.
- An `Examples:` block without a preceding `Scenario Outline:` is a parse error.

**Step registration:** Step definitions remain unchanged — register them normally with `RegisterGiven()`, `RegisterWhen()`, etc. The expanded scenarios are ordinary Scenarios from the registry's perspective; the templating is a pure parse-time concern.

### 7 — Timeout annotations: cooperative inter-step deadline checking

```gherkin
Feature: Library hold pickup

  @timeout:2s
  Scenario: Patron collects held book within service-level deadline
    Given a hold for the patron is waiting at the desk
    When the patron arrives and requests their hold
    Then the book is released within 2 seconds
    And the hold status is marked fulfilled
```

A scenario (or its Feature, inherited — Scenario-level wins if both set one) can carry a `@timeout:<value><unit>` tag to impose a wall-clock deadline for the entire scenario's execution. The unit is **REQUIRED** and must be one of: `s` (seconds), `ms` (milliseconds), or `m` (minutes). Examples of valid syntax: `@timeout:5s`, `@timeout:500ms`, `@timeout:2m`. Malformed values (missing unit, non-numeric value, unrecognized unit, zero or negative) are rejected — the scenario fails immediately before any Before hook, Background step, or Scenario step runs, with a clear malformed-annotation error message.

**Scenario vs. Feature precedence:** A timeout tag on a Scenario overrides any timeout tag on its parent Feature. If both set a timeout, only the Scenario's value is used. If only the Feature sets a timeout, all Scenarios in that Feature inherit it.

**Critical design limitation — cooperative, inter-step-only deadline checking:** This timeout is **NOT** preemptive or thread-based. The runtime does **not** spawn a separate thread or signal to forcibly interrupt a long-running step. Instead, before starting each step (Before hooks, Background steps, Scenario steps, and their respective failed-state sequels), the runtime checks whether elapsed wall-clock time since the scenario started already exceeds the timeout. If it does, that step (and every subsequent one) fails immediately with a "timeout exceeded" message, **without running the step's real body**. Once expired, every subsequent step also fails this way (each still produces its own distinct failed step result — BabyBehave's Gherkin mode always executes and reports every step, even after an earlier failure, matching this existing behavior documented [above](#5--forced-collect-failures-mode-and-its-consequence)). **This does NOT and CANNOT interrupt a single hung/blocking step already in progress.** There is deliberately NO preemptive threading or interruption — the design rationale is that `RunScenario` binds `const&` and `std::string_view` references to the caller's stack frame, so forcibly aborting a step from another thread would leave dangling references and undefined behavior. It only ever prevents the scenario from proceeding to its **next** step once the deadline has already passed. State plainly: **this is a deliberate, documented design limitation, not a bug or oversight.**

**After hooks always run:** After hooks execute to completion regardless of timeout expiry, mirroring BabyBehave's existing pattern of always running After hooks to guarantee cleanup even after earlier failures. The timeout check is bypassed for After hooks' execution.

**Typical integration with step registration:** No changes to step registration — timeout checking is entirely a parse-time and scenario-execution-time concern, invisible to step definitions. The same steps work with or without a `@timeout` tag.

### 8 — Parallel scenario execution: concurrent Scenario runs within a Feature

```cpp
// Custom non-exiting callback that collects failures safely under concurrent access
std::vector<std::string> failures;
std::mutex failuresMutex;

auto collectFailure = [&](std::string_view msg) {
    std::lock_guard<std::mutex> lock(failuresMutex);
    failures.push_back(std::string(msg));
};

Gherkin::FeatureResult result = Gherkin::RunFeature(
    featureText, 
    registry, 
    "ConcurrentOrders",
    collectFailure,
    /*enableParallelScenarios=*/true  // Enable parallel execution
);
```

By default, `RunFeature()` executes scenarios sequentially — one after another, in the order they appear in the `.feature` text. The new fifth, optional `enableParallelScenarios` parameter (defaulting to `false`) enables **concurrent scenario execution**: every scenario in the Feature runs in parallel via `std::async`, with each scenario getting its own independent `BabyBehaveTest` instance and `TestContext`. Execution flow and result reporting are deterministic regardless of thread scheduling: `FeatureResult::scenarioResults` is always written back in **original declaration order** (the order scenarios appear in the `.feature` file), not in the order they finish, via pre-allocation of result slots before dispatch.

**Rationale:** Features with many independent scenarios (e.g. a bakery processing unrelated customer orders concurrently) can realize substantial wall-clock speedup on multi-core systems when scenarios contain I/O waits or naturally parallel work. Serial execution is the safe default and remains byte-identical for backward compatibility; opt-in parallelism is only for scenarios that are naturally independent and correctly isolated.

**Critical safety caveat #1 — the default `onFailure` callback is NOT SAFE with parallelism:** The library's default failure callback (`impl::DefaultGherkinFailureAction`) calls `std::exit(EXIT_FAILURE)`, which is **not thread-safe** — calling `std::exit()` from multiple threads concurrently is a data race and undefined behavior. If you enable `enableParallelScenarios=true`, you **MUST** supply your own custom `onFailure` callback that does **not** call `std::exit()`, `std::abort()`, or any other process-terminating function. Use a thread-safe mechanism like a mutex-protected vector or atomic counter to collect failures, and let `RunFeature()` return normally. The example at the top of this section shows the correct pattern: a `collectFailure` lambda guarded by `std::mutex`. Attempting to use the default callback with `enableParallelScenarios=true` will likely crash or deadlock due to concurrent `std::exit()` invocations.

**Critical safety caveat #2 — behavioral divergence in exception handling:** This is a fundamental, non-obvious difference between serial and parallel modes. In **serial mode**, if a scenario's `onFailure` callback throws an exception or doesn't return (e.g. calls `std::exit()`), execution of that scenario halts immediately, and **no subsequent scenarios in the Feature run** — the exception propagates up or the process exits. In **parallel mode**, all scenario tasks are dispatched to `std::async` **before any of them execute**. If one scenario's `onFailure` callback throws an exception, that exception is captured and rethrown when `std::future::get()` is called on that particular scenario's future, aborting the result-collection join loop. However, **the other scenarios that were already dispatched continue running to completion** — the `std::vector<std::future<...>>` destructor blocks and waits for every future to finish, ensuring no thread is abandoned even if an exception escapes. If any of those other scenarios also throw, their exceptions are discarded (only the first exception seen via `.get()` propagates). The key point: in parallel mode, you cannot reliably prevent later scenarios from running by throwing in an earlier scenario's callback — all scenario work is already in flight. Design your `onFailure` callback accordingly: if you need "fail fast" semantics in parallel mode, collect the failure and check `FeatureResult::allPassed` afterward, rather than relying on exceptions to stop the other threads.

**Critical safety caveat #3 — per-scenario hook closures run concurrently and capture their own closures:** When `enableParallelScenarios=true`, any per-scenario hooks registered via `AddBeforeHook(tags, fn)` or `AddAfterHook(tags, fn)` execute concurrently across scenario threads, each in its own thread. If a hook closure captures mutable state by reference or pointer (e.g., `[&sharedCounter]` or `[sharedMap.get()]`), that shared state is **the consumer's own synchronization responsibility** — the framework does not serialize or lock access. The framework's own internal state (step registry, scenario result slots) has no such sharing between scenarios; this caveat applies only to what your own hook closures capture and access. Design hook closures with care: if multiple scenarios' hooks access the same mutable state, guard it with a mutex or use thread-safe data structures.

**Critical safety caveat #4 — no thread-pool cap; large Outline tables can spawn hundreds of OS threads:** Under `enableParallelScenarios=true`, every scenario (including every data row of a `Scenario Outline` when expanded) gets its own `std::launch::async` task and OS thread. There is **no internal thread pool, no cap on concurrent threads**. A `Scenario Outline` with a large `Examples:` table (e.g., 100 rows) combined with parallel execution will spawn ~100 OS threads concurrently, consuming substantial stack memory (one OS thread stack per scenario) and potentially exhausting system resources. This is an undocumented, easily-overlooked resource-usage consideration. If you have many-row Outlines, test behavior under `enableParallelScenarios=true` carefully, or cap parallelism at a higher level (e.g., run the Feature with `enableParallelScenarios=false` if the table is known to be large, or redesign the Outline into a smaller set of more-diverse scenarios).

**Critical linking requirement for parallel scenarios:** Consumers who set `enableParallelScenarios=true` must link `Threads::Threads` in their own CMake target (via `find_package(Threads REQUIRED)` and `target_link_libraries(YourTarget PRIVATE Threads::Threads)`). The header alone does not ensure this linkage — it is the consumer's responsibility. Parallel scenarios use `std::async` internally, which requires platform threading support (e.g., `-pthread` on Unix-like systems). Omitting this link step will result in linker errors on some platforms or silent race conditions on others. This CMake requirement is documented internally for one example (`examples/CMakeLists.txt`'s `GherkinCustomFailureHandler` target) but is not called out in the library consumer documentation, causing confusion. If you enable parallel scenarios and see linker errors mentioning "thread" or "pthread", ensure your CMakeLists.txt includes the `Threads::Threads` dependency.

**Isolation safety argument:** Each scenario running in parallel gets its own fresh `BabyBehaveTest` instance, which owns its own `TestContext` — a unique `std::unordered_map` per scenario with zero cross-scenario shared mutable state. Step registration (`StepRegistry`) is read-only during `RunFeature()` (all registration finishes before the parallel dispatch), so concurrent `const` reads from different threads are safe. This is the same isolation argument used for the pre-existing [`examples/gherkin/GherkinMultiThreaded.cpp`](../../examples/gherkin/GherkinMultiThreaded.cpp) and [`examples/gherkin/GherkinLibraryConcurrentLending.cpp`](../../examples/gherkin/GherkinLibraryConcurrentLending.cpp) examples — read one of those for additional precedent and implementation details.

### 9 — Data Tables: tabular arguments to steps

A step can be immediately followed (no blank line separation) by a pipe-delimited table. A data table is a two-dimensional grid of text cells: the first row is the header (column names), and subsequent rows are data rows. The table is associated with the preceding step only — a blank line breaks the association.

```gherkin
Feature: Bakery bulk order with itemization

  Scenario: Order total from itemized line items
    Given I am ready to process a bulk order
    When I add items to the order:
      | item       | quantity | unit_price |
      | Croissant  | 10       | 2.50       |
      | Sourdough  | 5        | 4.00       |
    Then the order total is 45.00 EUR
```

**Syntax:** A table line has the form `|cell1|cell2|cell3|...` (pipes at the start and end). Whitespace around cell content is trimmed (leading/trailing spaces are removed). Each row must have the same number of cells; trailing blank rows are ignored by the parser.

**Step definition opt-in:** A step definition can optionally accept a trailing `const DataTable&` parameter (in addition to any placeholder captures like `{int}` or `{word}` it already has) to receive the table. This is purely additive — existing step signatures without a `DataTable` parameter are completely unaffected, and a step can have both placeholder captures and a `DataTable` parameter together.

**DataTable API:** The `const DataTable&` parameter provides:
- `RowCount()` — number of data rows (excluding the header)
- `Header()` — the first row as a vector of column names
- `Row(dataIdx)` — a 0-indexed data row as a vector of cell strings
- `Get(dataIdx, columnName)` — header-aware cell lookup; throws `std::invalid_argument` if the column name doesn't exist

**Parse errors (fail hard):**
- A table with no immediately-preceding eligible step is a parse error ("data table with no preceding step").
- A step that already has a raw argument (from a prior table block or a Doc String) hit by a second table is a parse error ("step already has an argument").

**Note:** Data Tables and Doc Strings (see below) share the same underlying plumbing (`RawArgument`, `RawArgumentKind`). Both are optional trailing step-argument mechanisms distinct from placeholder captures — a step can have at most one of the two.

**Limitation:** Pipe characters (`|`) inside Data Table cell content are **not** supported and have no escape mechanism (unlike Cucumber's `\|`). Attempting to include a literal pipe will misparse the row as having an extra cell, producing a confusing `"row has N cell(s), expected M (from header)"` error. Workaround: avoid literal `|` in cell content.

### 10 — Doc Strings: multi-line string arguments to steps

A step can be immediately followed (no blank line separation) by a `"""`-delimited block; every line between the opening and closing `"""` (exclusive) becomes a single, newline-joined `std::string` attached to that step. Like Data Tables, a Doc String is associated with the preceding step only.

```gherkin
Feature: Article publishing

  Scenario: Publish an article
    Given the article body is:
      """
      This is a multi-line
      doc string.

      It can contain blank lines too.
      """
    When the article is published
    Then it should be live
```

**Syntax:** The opening and closing delimiter is `"""` (Cucumber's alternate `'''` delimiter is not supported). Every line in between is literal content — none of it is interpreted as a tag, comment, table row, or keyword, however it happens to start (a Doc String may safely contain `|` or `#` characters).

**Indentation stripping:** The opening `"""` line's own leading-whitespace column is stripped from every content line ("smart" indentation, matching Cucumber). A content line with less leading whitespace than that has only its own leading whitespace stripped (never more). Given the example above (the `"""` at column 7), the resulting string is `"This is a multi-line\ndoc string.\n\nIt can contain blank lines too."` — with no leading indentation from the `.feature` file's own nesting.

**Step definition opt-in:** A step definition can optionally accept a trailing `const std::string&` parameter (in addition to any placeholder captures it already has) to receive the Doc String content. This is purely additive, and follows the exact same arity-based opt-in rule as Data Tables (a step can have placeholder captures plus ONE trailing `DataTable`-or-`std::string` parameter, never both).

**Parse errors (fail hard):**
- A `"""` block with no immediately-preceding eligible step is a parse error ("doc string with no preceding step").
- A step that already has a raw argument (from a Data Table or a prior Doc String) hit by a second `"""` block is a parse error ("step already has an argument").
- An opening `"""` with no matching closing `"""` before end-of-file is a parse error ("doc string is not closed"), reported against the opening line.

### 11 — Retry/flaky annotations: automatic re-attempts on failure

```gherkin
Feature: Bakery oven sensor readings

  @retry:3
  Scenario: Preheat check tolerates a transient sensor glitch
    Given the oven is warming up
    When I read the oven temperature
    Then the temperature reading should be stable and within range
```

A scenario (or its Feature, inherited — Scenario-level wins if both set one, same precedence rule as `@timeout`) can carry a `@retry:N` tag to allow up to `N` **total attempts** at running the scenario, stopping at the first attempt that succeeds. `N` is a **total attempt count, not a count of additional retries**: `@retry:1` means "run once, no retry" (identical to omitting the tag, since the default is exactly one attempt), and `@retry:3` means "try up to three times total" (an initial attempt plus up to two retries).

**Every attempt is a full, independent re-run:** Before hooks → Background → Scenario's own steps → After hooks, exactly as described in [Mapping rules](#mapping-rules) above, with a **fresh `TestContext`** constructed for each attempt. Nothing carries over between attempts — no state, no partial progress, no shared mutable data. This is a deliberate design choice: a genuinely flaky dependency (a shaky sensor, a network call, a UI element that takes a moment to render) needs a clean slate on each attempt to be a fair, independent trial, rather than continuing from a possibly-corrupted mid-scenario state left behind by the previous failed attempt.

**Only the final outcome is reported:** `onFailure`/`FeatureResult::scenarioResults` only ever sees the outcome of the **last** attempt made — the first attempt that succeeds (if any), or the final attempt's failure if every attempt fails. Intermediate failed attempts are retried silently and never surface as their own reported result; they are an internal implementation detail of the retry loop, not additional scenario runs a consumer needs to reason about.

**Error cases (both fail hard, before any Before hook/Background/step runs):**
- `@retry:0` — an explicit zero is rejected; `@retry:N` is a *total* attempt count and zero attempts makes no sense.
- Negative or non-numeric values (e.g. `@retry:-1`, `@retry:abc`) — rejected with the same "malformed annotation" error style as `@timeout`.

**Interaction with `@timeout`:** Each attempt gets its **own fresh deadline** — a timeout that expires on attempt 1 does not eat into attempt 2's budget. `@retry:3 @timeout:2s` means each of up to three attempts individually has 2 seconds to complete, not the whole retry loop.

**Interaction with parallel execution (`enableParallelScenarios=true`):** Retries happen transparently inside each scenario's own async dispatch task — the retry loop is entirely local to one scenario's `std::async` task, so it composes with parallel scenario execution with no special-casing. `impl::InvokeOnFailure`'s mutex-guarded wrapper remains the sole path to the actual `onFailure` callback, so the thread-safety caveats in [Parallel scenario execution](#8--parallel-scenario-execution-concurrent-scenario-runs-within-a-feature) above apply unchanged.

**Caution — retries are only safe for idempotent/read-only steps:** Because every attempt fully re-runs Before hooks, Background, and Scenario steps, any step with a **side effect** (writing to a database, sending an email, charging a card, appending to a file) executes **once per attempt**, not once total. A step that charges a customer's card, wrapped in `@retry:3`, could charge them up to three times if the failure happens *after* the charge succeeds but before a later assertion. `@retry` should be reserved for scenarios whose steps are idempotent or purely read-only (e.g. polling a flaky sensor, waiting on eventual consistency) — never for scenarios with non-idempotent side effects, unless those side effects are themselves safely idempotent (e.g. an upsert keyed by a stable ID).

**Step registration:** No changes to step registration — retry is entirely a parse-time and scenario-execution-time concern, invisible to step definitions. The same steps work with or without a `@retry` tag.

See [`examples/gherkin/GherkinBakeryFlakyOvenSensorRetry.cpp`](../../examples/gherkin/GherkinBakeryFlakyOvenSensorRetry.cpp) and [`examples/gherkin/features/bakery_flaky_oven_sensor_retry.feature`](../../examples/gherkin/features/bakery_flaky_oven_sensor_retry.feature) for a complete example: a bakery oven temperature sensor that deterministically fails its first two read attempts (via a static counter, no randomness) before succeeding on the third, tagged `@retry:3`.

### 12 — Tag expressions (AND/OR/NOT): conditional hooks based on boolean tag matching

```cpp
registry.AddBeforeHookExpr("@smoke and not @slow", [](TestContext& ctx) {
    ctx.Set("perfCritical", true);
    // Fast-path setup only for @smoke scenarios without @slow
});

registry.AddAfterHookExpr("@vip or @premium", [](TestContext& ctx) {
    // Cleanup/logging specific to VIP or premium customers
});
```

A hook can now be registered with a **boolean tag-matching expression** in addition to the pre-existing vector-of-tags mechanism. The two methods `AddBeforeHookExpr(std::string expression, HookFunction fn)` and `AddAfterHookExpr(std::string expression, HookFunction fn)` accept an expression as a `std::string` and register a hook that fires against any Scenario whose tags make the expression evaluate to `true`.

**Expression grammar:**

- **Keywords** (case-insensitive): `and`, `or`, `not`
- **Tag syntax**: `@tagname` (leading `@` is required in the expression string; stripped when matched against Scenario tags using the same convention as `.feature` file parsing)
- **Parentheses**: `(...)` for grouping
- **Precedence** (highest to lowest): `not` > `and` > `or`
- **Examples**:
  - `"@smoke and not @slow"` — matches Scenarios tagged `@smoke` but not `@slow`
  - `"@vip or @premium"` — matches Scenarios tagged either `@vip` or `@premium`
  - `"not (@wip or @skip)"` — matches Scenarios that are neither `@wip` nor `@skip`
  - `"@integration and (@aws or @gcp)"` — matches Scenarios tagged `@integration` and either `@aws` or `@gcp`

**Registration-time parsing and fail-fast:**

Malformed expressions are detected and rejected immediately at the call to `AddBeforeHookExpr()` or `AddAfterHookExpr()`, **not later** during scenario execution. Examples of errors:

- Empty string: `""`
- Unbalanced parentheses: `"(@smoke and not @slow"`
- Missing operand: `"@smoke and and @slow"`, `"or @tag"`
- Unknown keyword: `"@smoke xor @slow"`
- Tag without `@`: `"smoke or @slow"`

All throw `std::invalid_argument` with a descriptive message at registration time. No hook fires that wasn't successfully registered.

**Why not same-named overloads like `AddBeforeHook(std::string, HookFunction)`?**

The existing `AddBeforeHook(const std::vector<std::string>&, HookFunction)` method is widely used at existing call sites like `AddBeforeHook({}, fn)` (empty vector = always runs) or `AddBeforeHook({"smoke", "fast"}, fn)`. Introducing a same-named overload `AddBeforeHook(const std::string&, ...)` would create ambiguity and silent behavioral changes:
- `AddBeforeHook("literal-tag-string", fn)` might be intended as a vector-based hook with one tag (now an error since vectors accept varargs, not strings), or might be intended as an expression-based hook if we added an overload. This ambiguity could silently break existing code.
- To preserve both semantics without collision, the new methods are named distinctly: `AddBeforeHookExpr` and `AddAfterHookExpr`.

**Mixing vector-based and expression-based hooks:**

A registry can freely mix both kinds of hooks with zero interference. Each hook is either vector-tags-based (via `AddBeforeHook`/`AddAfterHook`) **or** expression-based (via `AddBeforeHookExpr`/`AddAfterHookExpr`), never both. Execution order respects registration order across **both** kinds: if you register a vector-based hook, then an expression-based hook, then another vector-based hook, they fire in that order (all matching hooks fire before proceeding to the next Scenario step, in registration order). This is identical to the pre-existing hook execution semantics.

**No changes to step registration or `.feature` syntax:**

Tag expressions are purely a C++ registration-time concept. Step definitions registered via `RegisterGiven()`, `RegisterWhen()`, etc. are unchanged. Scenarios in `.feature` files use the standard `@tag` syntax (which they already have) — the expression evaluation is computed at hook-dispatch time based on a Scenario's accumulated tags (Feature-level + Scenario-level), with no new grammar or file syntax required.



### 13 — Suite-level Before/After-all hooks: one-time setup/teardown across all Scenarios

```cpp
registry.AddBeforeAllHook([]() {
    // Preheat the oven exactly once, before any Scenario runs
    OvenInstance.Preheat(200);
});

registry.AddAfterAllHook([]() {
    // Cool down exactly once, after all Scenarios finish
    OvenInstance.CoolDown();
});
```

A Feature can register suite-wide hooks that run exactly **once per Feature**, regardless of how many Scenarios the Feature contains: `AddBeforeAllHook(SuiteHookFunction)` runs before the first Scenario's Before hooks, and `AddAfterAllHook(SuiteHookFunction)` runs after the last Scenario's After hooks (including all Retry attempts). The signature is `using SuiteHookFunction = std::function<void()>` — no arguments and no return value — because suite-wide hooks have no single Scenario context to inspect: they operate at Feature scope, where per-Scenario data (tags, context) doesn't exist.

**Registration-order execution:** Multiple Before-ALL hooks run in the order they were registered, before any Scenario executes. Similarly, multiple After-ALL hooks run in registration order, after every Scenario has finished (including all Retry attempts). This matches the existing hook execution semantics for per-Scenario Before/After hooks.

**Unconditional execution (no tag filtering):** Unlike per-Scenario Before/After hooks (registered via `AddBeforeHook`/`AddAfterHook` with a tag filter), suite-level hooks are unconditional — they always run if registered, with no tag-matching expression or filters. This is intentional: suite-wide state (like "oven is preheated") applies uniformly across all Scenarios, and per-Scenario tag filtering would be meaningless at suite scope.

**BeforeAll-hook exception propagation:** If a `BeforeAll` hook throws an exception, the exception propagates straight out of `RunFeature()` uncaught — no subsequent scenarios run, and **no `AfterAll` hooks execute either**. This is empirically verified behavior and is intentional: if suite-wide setup fails (e.g., oven won't preheat), there is no meaningful state to clean up, and continuing to run Scenarios is pointless. Unlike per-Scenario Before hooks (which run inside the try-catch loop for each Scenario), `BeforeAll` runs once before the entire loop and is not wrapped. This is a deliberate, asymmetric design. If you need to ensure `AfterAll` hooks run even after `BeforeAll` failure, wrap your `BeforeAll` closure in its own try-catch and store the error in shared state instead of throwing directly.

**Critical design limitation — the default `onFailure` callback breaks After-ALL guarantees:** This is the most important caveat. The library's default failure callback (`impl::DefaultGherkinFailureAction`) calls `std::exit(EXIT_FAILURE)` as soon as the first Scenario fails, which means **After-ALL hooks are NEVER executed under the default behavior**. If your Feature has multiple Scenarios and the first one fails, the process exits immediately without running: (1) any remaining Scenarios, (2) any registered After-ALL hooks, or (3) per-Scenario After hooks. This is different from per-Scenario After hooks (which are guaranteed to run even after earlier steps fail, because the Gherkin interpreter forces collect-failures mode internally). **After-ALL hooks are only guaranteed to run if you supply a custom `onFailure` callback that does NOT exit the process.** Use a thread-safe mechanism like a mutex-protected vector to collect failures, and let `RunFeature()` return normally; then inspect `FeatureResult::allPassed` afterward to decide your own exit behavior. See the [Parallel scenario execution](#8--parallel-scenario-execution-concurrent-scenario-runs-within-a-feature) section's example pattern for how to wire up a collecting callback correctly.

If you don't need After-ALL guarantees (or your test suite only has one Scenario per Feature), the default callback is fine. But if suite-wide cleanup is critical (releasing external resources, flushing logs, etc.), you **must** supply a non-exiting `onFailure` callback to ensure After-ALL hooks run to completion.

**Interaction with parallel scenario execution (`enableParallelScenarios=true`):** Before-ALL hooks always run serially on the main thread, **before any parallel Scenario dispatch begins**. This means any suite-wide state they establish (like preheating an oven) is safely visible to all parallel Scenarios — it's fully written before any Scenario thread reads it, with no data race. After-ALL hooks similarly run serially after every Scenario (including all Retry attempts) has fully finished and all `std::async` futures have been joined, never overlapping with in-flight Scenario execution. The same critical caveat applies: if any Scenario fails under the default `onFailure` callback, After-ALL hooks never run — only a custom, non-exiting callback guarantees they execute.

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
- See [`examples/gherkin/GherkinCustomFailureHandler.cpp`](../../examples/gherkin/GherkinCustomFailureHandler.cpp) for a full example that collects every failure message into a `std::vector<std::string>` instead of exiting, then decides its own process exit code from `FeatureResult::allPassed` at the end.

## v0.8.2: StepRegistry::Merge() for registry reuse

`StepRegistry` gained a `Merge()` method and explicitly-defaulted special member functions, letting a consumer build one shared "library" registry once (typically via a factory function returning a `StepRegistry` by value) and reuse it across many, differently-shaped tests instead of re-registering the same step definitions in every test file.

```cpp
void Merge(const StepRegistry& other);

StepRegistry() = default;
~StepRegistry() = default;
StepRegistry(const StepRegistry&) = default;
StepRegistry& operator=(const StepRegistry&) = default;
StepRegistry(StepRegistry&&) = default;
StepRegistry& operator=(StepRegistry&&) = default;
```

- **Copy semantics, not aliasing** — `Merge()` copies every step definition and hook from `other` into `*this` (appended after anything already registered). `*this` and `other` remain fully independent afterward: mutating one post-`Merge()` (registering an additional step, or `Merge()`-ing something else in) never affects the other. This is what makes "one shared registry, several call sites each layering on their own extra steps" safe — no call site can accidentally see another's later additions.
- **The special member functions were already implicit** (no user-declared constructor/destructor/copy/move existed before) — declaring them `= default` explicitly is documentation of an intentional, supported contract (every member, `impl::StepDefinition`'s thunk and `impl::Hook`, is already copyable), not a behavioral change. clang-tidy's `cppcoreguidelines-special-member-functions` check requires the destructor alongside copy/move once any of them is user-declared, hence `~StepRegistry() = default;` alongside the copy/move pair.
- **First-registered-wins on overlap, unchanged** — if both registries have a matching pattern for the same keyword, `TryMatch()`'s first-match-wins linear scan means whichever was registered (or merged in) *first* still wins. This is the exact same pre-existing behavior as registering a literal duplicate pattern directly on one registry; `Merge()` introduces nothing new here, it just makes it possible to end up with a duplicate across two registries instead of within one.
- **Typical shape**: a header exposes `MakeXStepRegistry()` returning a populated `StepRegistry` by value; each test builds its own registry from the factory and, optionally, `Merge()`s in a handful of test-specific step definitions on top:

```cpp
// SharedSteps.hpp
inline StepRegistry MakeXStepRegistry() {
    StepRegistry registry;
    registry.RegisterGiven(/* ... */);
    // ... ~10-15 step definitions covering the domain's common operations
    return registry;
}

// SomeTest.cpp
StepRegistry registry = MakeXStepRegistry();

StepRegistry extras;
extras.RegisterAnd("a test-specific step", /* ... */);
registry.Merge(extras);

RunFeature(featureText, registry, "SomeTest.cpp");
```

- See [`examples/gherkin/BakerySteps.hpp`](../../examples/gherkin/BakerySteps.hpp) and [`examples/gherkin/LibrarySteps.hpp`](../../examples/gherkin/LibrarySteps.hpp) for two complete shared step libraries (a bakery's custom cake ordering system, and a library's book lending system), each reused unmodified across three `GherkinBakery*.cpp`/`GherkinLibrary*.cpp` examples with genuinely different scenarios; `GherkinBakeryAllergenSubstitution.cpp` and `GherkinLibraryHoldsAndReservations.cpp` additionally demonstrate `Merge()` layering a file-specific step on top of the shared registry. Unlike the rest of this project's Gherkin examples, these seven read their scenario text from real, standalone `.feature` files under `examples/gherkin/features/` rather than an embedded C++ raw string literal - purely an example-level choice (`RunFeature()` still only ever sees a `std::string_view` and never touches the filesystem itself).
- Unit tests: `tests/test_Gherkin_Integration.cpp`'s `GherkinRegistryMerge` suite covers combining steps from both registries, post-`Merge()` independence in both directions, and first-registered-wins precedence on an ambiguous overlap.

### Registry reuse across threads

A shared, read-only `StepRegistry` is also safe to fan out across multiple *threads*, not just multiple sequential test files: [`examples/gherkin/GherkinLibraryConcurrentLending.cpp`](../../examples/gherkin/GherkinLibraryConcurrentLending.cpp) builds one `StepRegistry` up front, then runs four different branches' `.feature` files concurrently (via `std::async`) against that same instance. This is safe because all registration finishes on the main thread before any thread starts, and from that point on `RunFeature()` only calls `StepRegistry`'s `const` member functions (`TryMatch()`, `BeforeHooks()`, `AfterHooks()`) - concurrent `const` reads of an object nobody is mutating are safe, same as reading a `std::vector`/`std::string` from multiple threads. Each Scenario still gets its own private `TestContext` internally (`RunScenario()` constructs a fresh `BabyBehaveTest` per Scenario), so there's no shared *mutable* state despite the shared registry - contrast with [`examples/gherkin/GherkinMultiThreaded.cpp`](../../examples/gherkin/GherkinMultiThreaded.cpp), where every thread builds its own independent registry instead.

**Concurrency hazard, called out explicitly because it previews a v0.9.0 risk**: `RunFeature()`'s *default* `onFailure` callback (`impl::DefaultGherkinFailureAction`) prints and calls `std::exit()` - safe from one thread, but **not** safe to invoke concurrently from several scenario threads at once (a std::exit() race, and interleaved stderr output). `GherkinLibraryConcurrentLending.cpp` therefore passes an explicit, mutex-guarded `GherkinFailureCallback` that only ever appends to a shared `std::vector<std::string>` and never exits; each thread's `FeatureResult` is collected via `std::future::get()` and inspected back on the main thread once every thread has joined. This exact hazard is why any future v0.9.0 "parallel Scenario execution" feature would need to either mandate a non-exiting `onFailure` callback under concurrency, or provide its own thread-safe default - it is not solved by today's v0.8.2 `Merge()`/reuse work, only worked around at the example level.

## Backlog beyond v0.8.1

- Everything in the "❌ deferred" rows of the [feature coverage table](#gherkin-feature-coverage-shipped-features) (`Rule:`, i18n keywords) remains open backlog, to be scoped when there's a concrete need. (Scenario Outline/Examples, Data Tables, and Doc Strings shipped — see [Scenario Outline / Examples: data-driven scenario expansion](#6--scenario-outline--examples-data-driven-scenario-expansion), [Data Tables: tabular arguments to steps](#9--data-tables-tabular-arguments-to-steps), and [Doc Strings: multi-line string arguments to steps](#10--doc-strings-multi-line-string-arguments-to-steps) above.)
- README wording follow-up ("no Gherkin files to parse" → qualify for the opt-in default), small documentation-only task, not yet assigned.
