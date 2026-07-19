// Demonstrates BabyBehave/matchers.hpp: an OPTIONAL helper usable inside
// plain bool(TestContext&) step bodies to replace raw ==/</> comparisons
// with a fluent Expect(...).ToXxx(...) syntax that prints a descriptive
// actual-vs-expected message to std::cerr on failure, instead of bdd.hpp's
// generic "Precondition failed" / "Postcondition failed" / ... label.
//
// matchers.hpp does not require any change to bdd.hpp - it is called from
// inside step bodies exactly like any other helper function would be.

#include <BabyBehave/bdd.hpp>
#include <BabyBehave/matchers.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace BabyBehave::BDD;
using namespace BabyBehave::Matchers;

class AlarmClock {
public:
    void SetAlarmTime(int hour, int minute) {
        m_alarmHour = hour;
        m_alarmMinute = minute;
    }

    void SetCurrentTime(int hour, int minute) {
        m_hour = hour;
        m_minute = minute;
    }

    void AddSnoozeLabel(const std::string& label) {
        m_snoozeLabels.push_back(label);
    }

    int GetAlarmHour() const { return m_alarmHour; }
    int GetAlarmMinute() const { return m_alarmMinute; }
    int GetHour() const { return m_hour; }
    const std::vector<std::string>& GetSnoozeLabels() const { return m_snoozeLabels; }
    bool IsTimeToRing() const { return m_hour == m_alarmHour && m_minute == m_alarmMinute; }

private:
    int m_alarmHour = 0;
    int m_alarmMinute = 0;
    int m_hour = 0;
    int m_minute = 0;
    std::vector<std::string> m_snoozeLabels;
};

void WorkingAlarmClock(TestContext& context) {
    auto alarmClock = std::make_shared<AlarmClock>();
    alarmClock->SetAlarmTime(7, 0);
    alarmClock->AddSnoozeLabel("first-snooze");
    alarmClock->AddSnoozeLabel("second-snooze");
    context.Set("AlarmClock", std::move(alarmClock));
}

// Below: step bodies use Expect(...).ToXxx(...) instead of raw comparisons.
// Each one still returns bool, so it slots directly into With/When/Then/And
// exactly as before - matchers.hpp changes nothing about how bdd.hpp treats
// the step, only how much detail is printed when it fails.

bool AlarmSetAt7AM(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    return Expect(alarmClock->GetAlarmHour()).ToEqual(7)
        && Expect(alarmClock->GetAlarmMinute()).ToEqual(0);
}

bool TimeReaches7AM(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    alarmClock->SetCurrentTime(7, 0);
    return Expect(alarmClock->GetHour()).ToBeGreaterOrEqualTo(0);
}

bool AlarmWillRing(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    return Expect(alarmClock->IsTimeToRing()).ToBeTrue();
}

bool HasFirstSnoozeLabel(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    return Expect(alarmClock->GetSnoozeLabels()).ToContain(std::string("first-snooze"));
}

// Deliberately-failing check: the alarm was set for 7 AM (see
// WorkingAlarmClock above), not 8 AM, so this always returns false. The
// point is to show Expect(...).ToEqual(...) printing the actual (7) vs
// expected (8) value to std::cerr before doing so, instead of just a
// generic "Precondition failed".
bool AlarmIsWronglyExpectedAt8AM(TestContext& context) {
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    return Expect(alarmClock->GetAlarmHour()).ToEqual(8);
}

int main() {
    // Passing scenario: every Expect(...) check below succeeds.
    GivenA(WorkingAlarmClock)
        .With(AlarmSetAt7AM)
        .When(TimeReaches7AM)
        .Then(AlarmWillRing)
        .And(HasFirstSnoozeLabel);

    // Failing scenario: SetCollectFailuresMode(true) keeps bdd.hpp from
    // calling std::exit() on failure (its default), purely so this example
    // can keep running afterwards and report the outcome below; it plays no
    // role in matchers.hpp itself, which works identically either way.
    BabyBehaveTest wrongExpectationTest = GivenA(WorkingAlarmClock);
    wrongExpectationTest.SetCollectFailuresMode(true);
    wrongExpectationTest.With(AlarmIsWronglyExpectedAt8AM);
    const TestResult& result = wrongExpectationTest.Execute();

    std::cout << "wrongExpectationTest allPassed: " << std::boolalpha << result.allPassed << std::endl;

    return 0;
}
