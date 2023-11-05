#include <BabyBehave/bdd.hpp>

using namespace BabyBehave::BDD;

class Calculator {
    int m_result = 0;

public:
    void add(int a, int b) {
        m_result = a + b;
    }

    int getResult() const {
        return m_result;
    }
};

void TwoNumbers(TestContext& ctx) {
    auto calculator = std::make_shared<Calculator>();
    ctx.Set("CalculatorKey", calculator);
}

bool TheyAreAddedTogether(TestContext& ctx) {
    auto calculator = ctx.Get<std::shared_ptr<Calculator>>("CalculatorKey");
    calculator->add(7, 5);
    return true;
}

bool TheResultShouldBeTwelve(TestContext& ctx) {
    auto calculator = ctx.Get<std::shared_ptr<Calculator>>("CalculatorKey");
    return calculator->getResult() == 12;
}

int main() {
    GivenA(TwoNumbers)
        .With(TheyAreAddedTogether)
        .Then(TheResultShouldBeTwelve);
    return 0;
}
