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

void ACalculator(TestContext& ctx) {
	auto calculator = std::make_shared<Calculator>();
	ctx.Set("CalculatorKey", calculator);
}


auto TwoNumbers(int num1, int num2) {
    return [=](TestContext& ctx) {
            ctx.Set("Num1", num1);
            ctx.Set("Num2", num2);
            return true;
        };
}

bool TheyAreAddedTogether(TestContext& ctx) {
    auto num1 = ctx.Get<int>("Num1");
    auto num2 = ctx.Get<int>("Num2");
    auto calculator = ctx.Get<std::shared_ptr<Calculator>>("CalculatorKey");
    calculator->add(num1, num2);
    return true;
}

auto TheResultShouldBe(int result) {
	return [=](TestContext& ctx) {
			auto calculator = ctx.Get<std::shared_ptr<Calculator>>("CalculatorKey");
			auto equals = calculator->getResult() == result;
            return equals;
		};
}

int main() {
    GivenA(ACalculator)
        .With(TwoNumbers(2, 9))
        .When(TheyAreAddedTogether)
        .Then(TheResultShouldBe(11));
    return 0;
}
