Feature: Bakery standard cake order fulfillment

  Scenario: Birthday cake order paid in full
    Given a new custom cake order
    And the cake is a "vanilla" cake serving 20 guests
    When a deposit of 50.00 is paid
    Then the total price should be 50.00
    And the deposit paid should be 50.00
    But no allergen substitutions should be recorded
