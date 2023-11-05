#include <BabyBehave/bdd.hpp>
#include <iostream>

using namespace BabyBehave::BDD;

class FlightBooking {
public:
    bool BookFlight() {
        return true;
    }

    bool SendConfirmationEmail() {
        return true;
    }
};

void GivenAFlightBookingWebsite(TestContext& context) {
    auto flightBooking = std::make_shared<FlightBooking>();
    context.Set("FlightBooking", std::move(flightBooking));
    context.Set("UserSelectedFlightPreferences", true);
    context.Set("PassengerDetailsEntered", true);
}

bool UserSelectsTheirFlightPreferences(TestContext& context) {
    auto userSelectedFlightPreferences = context.Get<bool>("UserSelectedFlightPreferences");
    if (userSelectedFlightPreferences) {
        return true;
    }
    std::cout << "User selects their flight preferences" << std::endl;
    context.Set("UserSelectedFlightPreferences", true);
    return true;
}

bool EntersPassengerDetails(TestContext& context) {
    auto passengerDetailsEntered = context.Get<bool>("PassengerDetailsEntered");
    context.Set("PassengerDetailsEntered", true);
    return true;
}

bool ClicksTheBookNowButton(TestContext& context) {
    auto flightBooking = context.Get<std::shared_ptr<FlightBooking>>("FlightBooking");
    auto bookingSuccessful = flightBooking->BookFlight();
    return bookingSuccessful;
}

bool TheFlightReservationShouldBeConfirmed(TestContext& context) {
    auto flightBooking = context.Get<std::shared_ptr<FlightBooking>>("FlightBooking");
    auto bookingSuccessful = flightBooking->BookFlight();
    auto emailSent = flightBooking->SendConfirmationEmail();
    return (bookingSuccessful && emailSent);
}

bool TheUserShouldReceiveABbookingConfirmationEmail(TestContext& context) {
    auto flightBooking = context.Get<std::shared_ptr<FlightBooking>>("FlightBooking");
    auto emailSent = flightBooking->SendConfirmationEmail();
    return emailSent;
}

int main() {
    GivenA(GivenAFlightBookingWebsite)
        .When(UserSelectsTheirFlightPreferences)
        .And(EntersPassengerDetails)
        .And(ClicksTheBookNowButton)
        .Then(TheFlightReservationShouldBeConfirmed)
        .And(TheUserShouldReceiveABbookingConfirmationEmail);
    return 0;
}