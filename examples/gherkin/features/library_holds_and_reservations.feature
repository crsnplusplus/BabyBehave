Feature: Library holds and reservations

  Background:
    Given the library has a copy of "Dune"
    And Alice is a library member in good standing
    And Bob is a library member in good standing

  Scenario: A hold is fulfilled once the book is returned
    When Alice checks out "Dune"
    And Bob places a hold on "Dune"
    Then the hold queue should contain 1 member
    And Bob should be at the front of the hold queue
    When Alice returns the book after 5 days
    And the hold for Bob is fulfilled and the book is checked out to them
    Then the book should be checked out to Bob
    And the library notifies Bob that "Dune" is ready for pickup
