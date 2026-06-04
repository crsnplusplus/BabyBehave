# BabyBehave

A minimalistic, header-only BDD framework for modern C++.

Write behavior-driven tests as plain C++ functions and compose them with a fluent `Given / With / When / Then` API. No dependencies, no code generation, no Gherkin files to parse — the test *is* the specification, and the readable output comes for free from your function names.

```text
Given a: FreshlyBootedCoffeeMachine
    With: AFullWaterTank
    When: IBrewAnEspresso
    Then: ACupIsServed
    And: TheTankLevelDecreases
```

## Features

- **Header-only**: drop `include/BabyBehave/bdd.hpp` into your project and you're done
- **Modern C++20**: built on concepts, `std::variant` and type-safe step dispatch
- **Zero dependencies**: standard library only
- **Fluent BDD vocabulary**: `Given`, `With`, `When`, `Then`, `And`, `Or`, `But` (plus `GivenA`, `WhenI`, `ThenI`, … variants for readable English)
- **Shared test context**: a type-safe key/value store passes state between steps
- **Customizable failure handling**: plug in your own callbacks for failed conditions and exceptions
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
- Each chained step (`With`, `When`, `Then`, `And`, `Or`, `But`) registers a typed step (`Precondition`, `Action`, `Postcondition`, …).
- The scenario executes when the test object goes out of scope; steps run in order and each one's return value is verified.
- Step types are dispatched at compile time through C++20 concepts, so each keyword gets its own labelled output and failure message.

### TestContext

`TestContext` is a type-safe `std::any`-based store shared across all steps of a scenario:

```cpp
ctx.Set("answer", 42);
int x = ctx.Get<int>("answer");   // throws std::out_of_range if the key is missing
```

### Failure handling

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

## Building the examples and tests

```bash
git clone https://github.com/crsnplusplus/BabyBehave.git
cd BabyBehave
cmake -B build
cmake --build build
```

See the [`examples/`](examples/) and [`tests/`](tests/) directories for working scenarios.

## Requirements

- A C++20 compiler (the framework relies on concepts)
- CMake 3.x only if you want to build the examples/tests; consuming the header requires nothing

## A note on the macros

The fluent keywords are implemented as macros (`Given`, `When`, `Then`, `And`, `Or`, `But`, …) so they can stringify your function names. They use capitalized identifiers to stay clear of the C++ alternative tokens `and`/`or`, but if a name collides with something in your codebase, you can always call the underlying API directly: `GivenAImpl("name", fn)` and `AddStep<Precondition>("name", fn)`.

## License

[MIT](LICENSE) — © 2023 Cristian Marletta
