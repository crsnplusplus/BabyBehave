// Demonstrates opting out of BabyBehave's short macros (Given/With/When/Then/And/Or/But/...)
// by defining BABYBEHAVE_NO_SHORT_MACROS before including the header. Only the explicit
// AddStep<StepType>(...) / GivenAImpl(...) API is used below.
#define BABYBEHAVE_NO_SHORT_MACROS
#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

void EmptyBasket(TestContext& context) {
    context.Set<int>("ItemCount", 0);
}

bool BasketIsEmpty(TestContext& context) {
    return context.Get<int>("ItemCount") == 0;
}

bool AddItemToBasket(TestContext& context) {
    context.Set<int>("ItemCount", context.Get<int>("ItemCount") + 1);
    return true;
}

bool BasketHasOneItem(TestContext& context) {
    return context.Get<int>("ItemCount") == 1;
}

int main() {
    GivenAImpl("an empty basket", EmptyBasket)
        .AddStep<Precondition>("BasketIsEmpty", BasketIsEmpty)
        .AddStep<Action>("AddItemToBasket", AddItemToBasket)
        .AddStep<Postcondition>("BasketHasOneItem", BasketHasOneItem);

    return 0;
}
