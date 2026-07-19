Feature: Library concurrent lending - Harbor branch

  Scenario: Returning exactly on the due date owes no fine
    Given the library has a copy of "Educated"
    And Hana is a library member in good standing
    When Hana checks out "Educated"
    And Hana returns the book after 14 days
    Then the book should be available
    And Hana should not owe any fine
