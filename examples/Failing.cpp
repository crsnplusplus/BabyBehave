#include <BabyBehave/bdd.hpp>
#include <stdexcept>

using namespace BabyBehave::BDD;

void FailingTest(TestContext& context) {
    throw std::runtime_error("This test always fails");
}

int main() {
    GivenA(FailingTest);

    return 0;
}