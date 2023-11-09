#include <BabyBehave/bdd.hpp>
#include <chrono>

using namespace BabyBehave::BDD;

class Oven {
public:
    void SetTargetTemperature(int temperature) {
        m_setTargetTemperature = temperature;
    }

    bool IsTemperatureReached() {
        return m_temperature == m_setTargetTemperature;
    }

    void SetCurrentTemperature(int temperature) {
		m_temperature = temperature;
	}

    void PowerOn() {
        m_powerOn = true;
    }

    bool IsPowerOn() const {
        return m_powerOn;
    }

private:
    int m_temperature = 0;
    int m_setTargetTemperature = 0;
    std::string m_product;
    bool m_powerOn = false;
};

void WorkingOven(TestContext& context) {
    auto oven = std::make_shared<Oven>();
    context.Set("Oven", std::move(oven));
}

bool PowerOff(TestContext& context) {
    auto oven = context.Get<std::shared_ptr<Oven>>("Oven");
    oven->PowerOn();
    return true;
}

bool TurnOnTheOven(TestContext& context) {
    auto oven = context.Get<std::shared_ptr<Oven>>("Oven");
    oven->PowerOn();
    return true;
}

auto SetTemperatureAt(int temperature) {
    return[=](TestContext& context) -> bool {
        auto oven = context.Get<std::shared_ptr<Oven>>("Oven");
        oven->SetTargetTemperature(temperature);
        return true;
    };
}

auto TheOvenIsTurnedOn(TestContext& context) -> bool {
    auto oven = context.Get<std::shared_ptr<Oven>>("Oven");
    return oven->IsPowerOn();
}

auto TheTemperatureIsReachedIn(std::chrono::seconds seconds) {
    return[=](TestContext& context) -> bool {
        auto oven = context.Get<std::shared_ptr<Oven>>("Oven");
        std::this_thread::sleep_for(seconds);
        oven->SetCurrentTemperature(200);
        return oven->IsTemperatureReached();
    };
}

int main() {
    GivenA(WorkingOven)
        .With(PowerOff)
        .WhenI(TurnOnTheOven)
        .AndI(SetTemperatureAt(200))
        .Then(TheOvenIsTurnedOn)
        .And(TheTemperatureIsReachedIn(std::chrono::seconds(5)));

    return 0;
}
