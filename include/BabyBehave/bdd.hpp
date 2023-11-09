#ifndef BABYBEHAVE_BDD_HPP
#define BABYBEHAVE_BDD_HPP

#include <functional>
#include <vector>
#include <variant>
#include <memory>
#include <unordered_map>
#include <any>
#include <string>
#include <stdexcept>
#include <concepts>
#include <iostream>


namespace BabyBehave::BDD {

    class TestContext {
    private:
        std::unordered_map<std::string, std::any> m_objects;

    public:
        template<typename T>
        void Set(const std::string& key, T obj) {
            m_objects[key] = obj;
        }

        template<typename T>
        T Get(const std::string& key) {
            auto it = m_objects.find(key);
            if (it == m_objects.end()) {
                auto errorMsg = "Key not found: " + key;
                std::cout << errorMsg << std::endl;
                throw std::runtime_error(errorMsg);
            }
            return std::any_cast<T>(it->second);
        }
    };

    using StepFunction = std::function<bool(TestContext&)>;

    struct Precondition { StepFunction fn; };
    struct Action { StepFunction fn; };
    struct Postcondition { StepFunction fn; };
    struct And { StepFunction fn; };
    struct Or { StepFunction fn; };
    struct But { StepFunction fn; };

    template<typename T>
    concept IsPrecondition = std::same_as<T, Precondition>;

    template<typename T>
    concept IsAction = std::same_as<T, Action>;

    template<typename T>
    concept IsPostcondition = std::same_as<T, Postcondition>;

    template<typename T>
    concept IsAnd = std::same_as<T, And>;

    template<typename T>
    concept IsOr = std::same_as<T, Or>;

    template<typename T>
    concept IsBut = std::same_as<T, But>;


    class BabyBehaveTest {
    public:
        using StepVariant = std::variant<Precondition, Action, Postcondition, And, Or, But>;
        using Step = std::pair<std::string, StepVariant>;


        BabyBehaveTest(const std::string& testName, std::function<void(TestContext&)> contextSetupFn)
            : m_testName(testName),
            m_contextSetupFn(contextSetupFn) {
            m_onConditionNotVerifiedCallback = [](const std::string& errorMsg) {
                std::cerr << errorMsg << std::endl;
                std::exit(EXIT_FAILURE);
            };
            m_onExceptionCallback = [](const std::string& step, const std::exception& e) {
                std::cerr << "Exception caught in " << step << ":" 
                          << std::string(e.what()) << std::endl;
                std::exit(EXIT_FAILURE);
            };
        }

        ~BabyBehaveTest() {
            Execute();
        }

        void SetOnConditionNotVerifiedCallback(std::function<void(const std::string& errorMsg)> callback) {
            m_onConditionNotVerifiedCallback = callback;
        }

        void SetOnExceptionCallback(std::function<void(const std::string& msg, const std::exception&)> callback) {
            m_onExceptionCallback = callback;
        }

        // Definizione del template per aggiungere qualsiasi tipo di step
        template<typename StepType>
        BabyBehaveTest& AddStep(const std::string& name, const typename StepFunction& stepFunction) {
            StepVariant step = StepType{ stepFunction };
            m_steps.push_back({ name, step });
            return *this;
        }

    private:
        void Execute() {
            std::cout << "Given a: " << m_testName << std::endl;
            try {
                m_contextSetupFn(m_context);
            }
            catch (const std::exception& e) {
                VerifyCondition(false, "Exception caught in Context Setup: " + std::string(e.what()));
            }

            for (const auto& step : m_steps) {
                std::visit([this, &step](auto&& arg) {
                    executeStep(step.first, arg);
                    }, step.second);
            }

            std::cout << std::endl;
        }

        template<typename T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    " << typeid(T).name() << ": " << name << std::endl;
            try {
                VerifyCondition(step(m_context), typeid(T).name() + " failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Exception caught in " + typeid(T).name() + ": " + std::string(e.what()));
            }
        }


        template<IsPrecondition T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    With: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Precondition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Precondition", e);
            }
        }

        template<IsAction T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    When: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Action failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Action", e);
            }
        }

        template<IsPostcondition T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    Then: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Precondition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Precondition", e);
            }
        }

        template<IsAnd T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    And: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "And condition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("And condition" , e);
            }
        }

        template<IsOr T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    Or: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "Or condition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("Or condition", e);
            }
        }

        template<IsBut T>
        void executeStep(const std::string& name, const T& step) {
            std::cout << "    But: " << name << std::endl;
            try {
                VerifyCondition(step.fn(m_context), "But condition failed");
            }
            catch (const std::exception& e) {
                m_onExceptionCallback("But condition", e);
            }
        }


        void VerifyCondition(bool condition, const std::string& errorMsg) {
            if (!condition) {
                m_onConditionNotVerifiedCallback(errorMsg);
            }
        }

    private:
        std::string m_testName;
        std::function<void(TestContext&)> m_contextSetupFn;
        TestContext m_context;
        std::vector<Step> m_steps;

        std::function<void(const std::string& errorMsg)> m_onConditionNotVerifiedCallback;
        std::function<void(const std::string& step, const std::exception&)> m_onExceptionCallback;
    };


    inline BabyBehaveTest GivenAImpl(const std::string& testName,
        std::function<void(TestContext&)> contextSetup) {
        return BabyBehaveTest(testName, contextSetup);
    }

#define Given(func)  GivenAImpl(#func, {func})
#define GivenA(func) GivenAImpl(#func, {func})
#define With(func)  AddStep<Precondition>(#func, {func})
#define WithI(func) AddStep<Precondition>(#func, {func})
#define When(func)  AddStep<Action>(#func, {func})
#define WhenI(func) AddStep<Action>(#func, {func})
#define Then(func)  AddStep<Postcondition>(#func, {func})
#define ThenI(func) AddStep<Postcondition>(#func, {func})
#define And(func)   AddStep<And>(#func, {func})
#define AndI(func)  AddStep<And>(#func, {func})
#define But(func)   AddStep<But>(#func, {func})
#define ButI(func)  AddStep<But>(#func, {func})
#define Or(func)    AddStep<Or>(#func, {func})
#define OrI(func)   AddStep<Or>(#func, {func})
} // namespace BabyBehave::BDD

#endif // BABYBEHAVE_BDD_HPP
