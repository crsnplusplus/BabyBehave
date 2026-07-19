Feature: Library overdue fines

  Scenario: A book returned five days late accrues a fine
    Given the library has a copy of "Foundation"
    And Carol is a library member in good standing
    When Carol checks out "Foundation"
    And Carol returns the book after 19 days
    Then Carol should owe a fine of 1.25
    And the book should be available
