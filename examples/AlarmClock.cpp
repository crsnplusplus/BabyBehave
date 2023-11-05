#include <BabyBehave/bdd.hpp>
#include <iostream>


using namespace BabyBehave::BDD;

class AlarmClock {
public:
    void SetAlarmTime(int hour, int minute) {
        m_alarmHour = hour;
        m_alarmMinute = minute;
    }

    bool IsTimeToRing() {
        return m_hour == m_alarmHour && m_minute == m_alarmMinute;
    }

    void SetCurrentTime(int hour, int minute) {
        m_hour = hour;
        m_minute = minute;
    }

private:
    int m_alarmHour = 0;
    int m_alarmMinute = 0;
    int m_hour = 0;
    int m_minute = 0;
};

void WorkingAlarmClock(TestContext& context) {
    auto alarmClock = std::make_shared<AlarmClock>();
    alarmClock->SetAlarmTime(7, 0);
    context.Set("AlarmClock", std::move(alarmClock));
}

bool AlarmSetAt7AM(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    alarmClock->SetAlarmTime(7, 0);
    return true;
}

auto AlarmSetAt(int hour, int minute) {
    return[=](TestContext& context) -> bool {
        auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
        alarmClock->SetAlarmTime(hour, minute);
        return true;
    };
}


bool TimeSetAt9PM(TestContext& context) {
    return true;
}

void TimeReaches7AM(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    alarmClock->SetCurrentTime(7, 0);
}

bool AlarmWillRing(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    return alarmClock->IsTimeToRing();
}

int main() {
    GivenA(WorkingAlarmClock)
        .With(AlarmSetAt7AM)
        .With(TimeSetAt9PM)
        .When(TimeReaches7AM)
        .Then(AlarmWillRing);

    GivenA(WorkingAlarmClock)
        .With(AlarmSetAt(7,0))
        .With(TimeSetAt9PM)
        .When(TimeReaches7AM)
        .Then(AlarmWillRing);

    return 0;
}
