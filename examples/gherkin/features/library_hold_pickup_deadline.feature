Feature: Library hold pickup deadline

  Background:
    Given the library has a copy of "The Midnight Library"
    And Alice is a library member in good standing
    And Bob is a library member in good standing

  @timeout:2s
  Scenario: A patron can pick up a held book within service window
    When Alice checks out "The Midnight Library"
    And Bob places a hold on "The Midnight Library"
    And Alice returns the book after 2 days
    And the hold for Bob is fulfilled and the book is checked out to them
    Then the book should be checked out to Bob
