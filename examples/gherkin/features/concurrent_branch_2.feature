Feature: Library concurrent lending - Riverside branch

  Background:
    Given the library has a copy of "Sapiens"
    And Farah is a library member in good standing
    And Grace is a library member in good standing

  Scenario: A hold is fulfilled while other branches run at the same time
    When Farah checks out "Sapiens"
    And Grace places a hold on "Sapiens"
    Then the hold queue should contain 1 member
    And Grace should be at the front of the hold queue
    When Farah returns the book after 3 days
    And the hold for Grace is fulfilled and the book is checked out to them
    Then the book should be checked out to Grace
