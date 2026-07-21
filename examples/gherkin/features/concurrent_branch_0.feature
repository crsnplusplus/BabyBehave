Feature: Library concurrent lending - Uptown branch

  Scenario: A book checked out and returned on time
    Given the library has a copy of "Silent Spring"
    And Dana is a library member in good standing
    When Dana checks out "Silent Spring"
    And Dana returns the book after 7 days
    Then the book should be available
    And Dana should not owe any fine
