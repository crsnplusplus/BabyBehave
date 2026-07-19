Feature: Library concurrent lending - Downtown branch

  Scenario: A book returned twenty days late accrues a larger fine
    Given the library has a copy of "1984"
    And Evan is a library member in good standing
    When Evan checks out "1984"
    And Evan returns the book after 20 days
    Then Evan should owe a fine of 1.5
    And the book should be available
