# BabyBehave Configuration Reference

This document lists every compile-time `#define` and runtime environment variable that BabyBehave recognizes. Most consumers can ignore these and use the defaults, but they are available for special cases (pure C++17 builds, CI/CD output formatting, namespace collisions, etc.).

## Compile-time `#define` options

Define any of these **before** `#include <BabyBehave/bdd.hpp>` to change compile-time behavior.

### `BABYBEHAVE_DISABLE_GHERKIN`

**Since:** v0.8.0  
**Default:** Not defined (Gherkin is ON by default)  
**Type:** Compile-time gate

Disables Gherkin support, removing the `BabyBehave::BDD::Gherkin` namespace and the C++20-specific `#include` directives (`<concepts>`, `<optional>`, etc.) that it depends on.

**When to use:** For projects targeting pure C++17 that do not need Gherkin `.feature` file parsing. The rest of BabyBehave's C++17-compatible API (fluent BDD macros, matchers, reporters) remains fully available.

**Example:**
```cpp
#define BABYBEHAVE_DISABLE_GHERKIN
#include <BabyBehave/bdd.hpp>

// BabyBehave::BDD::Gherkin is NOT defined; everything else works normally
using namespace BabyBehave::BDD;

bool MyStep(TestContext& ctx) { return true; }

int main() {
    Given(MyStep);
}
```

**Impact:** Removes ~1400 lines (roughly 30% of `bdd.hpp`'s total). Compile-time and runtime cost: zero when disabled (no template instantiations, no extra symbols, no extra `#include`s).

---

### `BABYBEHAVE_NO_SHORT_MACROS`

**Since:** v0.7.0  
**Default:** Not defined (short macros are defined)  
**Type:** Compile-time gate

Disables the short, generic macro aliases (`Given`, `With`, `When`, `Then`, `And`, `Or`, `But`, and their `-I` variants like `WithI`, `WhenI`, etc.).

**When to use:** Your codebase or another library already uses names like `When`, `Then`, `And`, etc., causing a collision. BabyBehave's underlying functions (`GivenAImpl()`, `AddStep<StepType>()`) are still available; you just use them directly instead of through the convenience macros.

**Example (no collision):**
```cpp
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

bool MyStep(TestContext& ctx) { return true; }

int main() {
    Given(MyStep).When(MyStep).Then(MyStep);  // Works
}
```

**Example (with collision):**
```cpp
#define BABYBEHAVE_NO_SHORT_MACROS
#include <BabyBehave/bdd.hpp>

// Some library also defines When, Then, etc. (no conflict now)

using namespace BabyBehave::BDD;

bool MyStep(TestContext& ctx) { return true; }

int main() {
    GivenAImpl("setup", MyStep)
        .AddStep<Precondition>("precondition", MyStep)
        .AddStep<Action>("action", MyStep)
        .AddStep<Postcondition>("postcondition", MyStep);
}
```

For a more detailed example, see [`examples/NoShortMacros.cpp`](../examples/NoShortMacros.cpp).

---

## Runtime environment variables

Set these at test execution time (via your test runner, shell, or `set_tests_properties()` in CMake) to control runtime behavior.

### `BABYBEHAVE_QUIET`

**Since:** v0.7.0  
**Default:** Not set (narration is enabled)  
**Type:** Runtime toggle  
**Valid values:** `"0"`, empty string, or any other string

Controls whether step narration (the human-readable output describing what each scenario is testing) is printed to `stdout`.

| Value | Behavior |
|-------|----------|
| Not set (unset) | Narration is printed (ON) |
| `""` (empty) | Narration is printed (ON) |
| `"0"` (zero) | Narration is printed (ON) |
| Any other string (including `"1"`, `"true"`, etc.) | Narration is silenced (OFF) |

**When to use:** In CI/CD pipelines where test output must be machine-readable or concise (e.g., before piping through `grep`, or in a logging system that already captures test names). Also useful when running many tests in parallel and the interleaved narration becomes unreadable.

**Example (bash):**
```bash
# Silence narration for this test run
BABYBEHAVE_QUIET=1 ./my_test_binary

# Or set it for ctest
ctest --test-dir build/bb-release -E ".*" -DBABYBEHAVE_QUIET=1
```

**Example (CMake `set_tests_properties`):**
```cmake
add_executable(my_test my_test.cpp)
set_tests_properties(my_test PROPERTIES
    ENVIRONMENT "BABYBEHAVE_QUIET=1"
)
```

**Note:** The `SetNarrationEnabled()` runtime API in `bdd.hpp` can override this flag at any point during test execution — useful if you want to enable/disable narration within your test code itself (e.g., for specific test categories).

---

### `BABYBEHAVE_STYLE`

**Since:** v0.7.0  
**Default:** `"plain"` (when unset)  
**Type:** Runtime selection  
**Valid values:** `"plain"`, `"arrow"`, `"tree"`, or unset

Selects which narration style BabyBehave uses when printing step output. All styles produce semantically identical results; they differ only in how they format the output.

| Style | Output format | Use case |
|-------|---------------|----------|
| `"plain"` (default) | Steps printed live as they run, one line per step | CI logs, real-time feedback, default |
| `"arrow"` | Buffered output with `->` prefix for each step, printed once per scenario | Compact, structured, Gherkin-like alignment |
| `"tree"` | Buffered output with a tree structure (primary steps at top level, detail steps nested) | Hierarchical visualization, reading comprehension |

**When to use:** Choose based on how you want the test output formatted. Most consumers can leave it unset and use the default `plain` style.

**Example (bash):**
```bash
# Use arrow style for this test run
BABYBEHAVE_STYLE=arrow ./my_test_binary

# Use tree style
BABYBEHAVE_STYLE=tree ./my_test_binary

# Unset (or set to anything else): defaults to plain
BABYBEHAVE_STYLE=unknown ./my_test_binary  # Uses "plain" (unrecognized values default to plain)
```

**Example (CMake):**
```cmake
add_executable(my_test my_test.cpp)
set_tests_properties(my_test PROPERTIES
    ENVIRONMENT "BABYBEHAVE_STYLE=tree"
)
```

**Note:** The `SetNarrationStyle()` runtime API in `bdd.hpp` can override this at any point during test execution.

---

## Summary table

| Name | Type | Default | Scope | Purpose |
|------|------|---------|-------|---------|
| `BABYBEHAVE_DISABLE_GHERKIN` | Compile-time `#define` | Not defined (Gherkin ON) | `bdd.hpp` only | Opt out of Gherkin support for C++17 builds |
| `BABYBEHAVE_NO_SHORT_MACROS` | Compile-time `#define` | Not defined (short macros ON) | `bdd.hpp` only | Resolve macro name collisions |
| `BABYBEHAVE_QUIET` | Environment variable | Unset (narration ON) | All test runs | Silence step narration output |
| `BABYBEHAVE_STYLE` | Environment variable | `"plain"` | All test runs | Select narration style (plain/arrow/tree) |
