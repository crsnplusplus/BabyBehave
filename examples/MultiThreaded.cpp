// Demonstrates the CORRECT, safe way to run multiple BabyBehave scenarios
// concurrently.
//
// Every GivenA(...) call creates a brand new BabyBehaveTest, and
// BabyBehaveTest owns its TestContext as a private member (see m_context in
// bdd.hpp) -- it is never shared unless a consumer goes out of their way to
// pass one TestContext& into several scenarios. Below, each call to
// RunAlarmScenario() builds its own AlarmClock, its own GivenA/With/When/Then
// chain, and therefore its own private TestContext, all independent of every
// other call. Because there is zero shared mutable state between scenarios,
// launching N of them in parallel -- one per thread, via std::async -- needs
// no locks, no atomics, no synchronization at all: there is nothing for the
// threads to race on.
//
// Contrast this with the #if 0 block near the bottom of this file, which
// sketches the UNSAFE pattern this example deliberately avoids: sharing a
// single TestContext across threads. That block is illustrative only and is
// never compiled.

#include <BabyBehave/bdd.hpp>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace BabyBehave::BDD;

namespace {

// Small per-step delay so concurrent scenarios visibly interleave their
// "Given a/With/When/Then" console output instead of one scenario racing
// to completion before the next one starts printing. Not required for
// correctness -- BabyBehave already prints step progress as it goes, so
// genuinely concurrent execution interleaves on its own -- this just makes
// that interleaving obvious to a human reading stdout.
constexpr auto kStepDelay = std::chrono::milliseconds(5);

void TinyDelay() {
    std::this_thread::sleep_for(kStepDelay);
}

class AlarmClock {
public:
    void SetAlarmTime(int hour, int minute) {
        m_alarmHour = hour;
        m_alarmMinute = minute;
    }

    bool IsTimeToRing() const {
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

auto AlarmClockWithAlarmAt(int hour, int minute) {
    return [=](TestContext& context) {
        auto alarmClock = std::make_shared<AlarmClock>();
        alarmClock->SetAlarmTime(hour, minute);
        context.Set("AlarmClock", std::move(alarmClock));
        TinyDelay();
    };
}

auto AlarmSetAt(int hour, int minute) {
    return [=](TestContext& context) -> bool {
        TinyDelay();
        auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
        alarmClock->SetAlarmTime(hour, minute);
        return true;
    };
}

auto TimeReaches(int hour, int minute) {
    return [=](TestContext& context) -> bool {
        TinyDelay();
        auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
        alarmClock->SetCurrentTime(hour, minute);
        return true;
    };
}

bool AlarmWillRing(TestContext& context) {
    TinyDelay();
    auto alarmClock = context.Get<std::shared_ptr<AlarmClock>>("AlarmClock");
    return alarmClock->IsTimeToRing();
}

// One fully self-contained scenario: its own AlarmClock, stored in its own
// GivenA(...)'s own private TestContext. Nothing here is shared with any
// other call to RunAlarmScenario(), whether that call runs on this thread
// or another one -- so calling this from N different threads at once is
// safe with zero synchronization.
void RunAlarmScenario(int hour, int minute) {
    GivenA(AlarmClockWithAlarmAt(hour, minute))
        .With(AlarmSetAt(hour, minute))
        .When(TimeReaches(hour, minute))
        .Then(AlarmWillRing);
}

} // namespace

int main() {
    constexpr int kScenarioCount = 4;

    std::vector<std::future<void>> futures;
    futures.reserve(kScenarioCount);

    for (int i = 0; i < kScenarioCount; ++i) {
        const int hour = 6 + i;
        const int minute = (i * 15) % 60;
        // Each scenario runs on its own thread with its own isolated
        // TestContext -- safe with no locks, because nothing is shared.
        futures.push_back(std::async(std::launch::async, RunAlarmScenario, hour, minute));
    }

    for (auto& fut : futures) {
        fut.get();
    }

    std::cout << kScenarioCount
              << " independent scenarios completed concurrently, each with its own "
                 "isolated TestContext -- no synchronization needed.\n";

    return 0;
}

#if 0
// -----------------------------------------------------------------------
// UNSAFE COUNTEREXAMPLE -- illustrative only. Deliberately guarded by
// #if 0 so it never compiles or runs as part of this example.
//
// TestContext is explicitly documented as NOT thread-safe (see the comment
// directly above the TestContext class in bdd.hpp): it is backed by a plain
// std::unordered_map with no internal synchronization. Nothing in the
// library's API stops a consumer from sharing a single TestContext&
// across multiple scenarios -- but doing so from multiple threads without
// external locking is undefined behavior. std::unordered_map is not safe
// for concurrent mutation: concurrent Set() calls can race on the same
// bucket or on a rehash, and even a Set() on one thread racing a Get() on
// another can observe a torn/corrupted map, crash, or silently return wrong
// data. This is exactly the scenario the thread-safety warning on
// TestContext is guarding against, so never do this:

TestContext sharedContext; // one instance shared across threads -- BAD

void UnsafeSetup(TestContext& context) {
    context.Set<int>("Counter", 0);
}

void UnsafeIncrement() {
    // Multiple threads calling this concurrently against the SAME
    // sharedContext race on the underlying std::unordered_map with no lock
    // protecting it -- this is undefined behavior, not merely "wrong
    // results".
    auto current = sharedContext.Get<int>("Counter");
    sharedContext.Set<int>("Counter", current + 1);
}

void UnsafeDemo() {
    UnsafeSetup(sharedContext);

    constexpr int kThreadCount = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreadCount; ++i) {
        // BAD: every thread mutates the same TestContext instance
        // concurrently, with no lock -- exactly what the class-level
        // thread-safety comment in bdd.hpp warns against.
        threads.emplace_back(UnsafeIncrement);
    }
    for (auto& t : threads) {
        t.join();
    }
    // sharedContext's Counter is not reliably 4 here, and worse, the
    // unordered_map itself can be left in a corrupted state -- this is a
    // data race, full stop.
}
#endif
