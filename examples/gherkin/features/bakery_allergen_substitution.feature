Feature: Bakery order with allergen substitution

  Scenario: Gluten-free substitution adds a surcharge
    Given a new custom cake order
    And the cake is a "chocolate" cake serving 10 guests
    When the customer requests a gluten-free allergen substitution
    And a deposit of 30.00 is paid
    Then the total price should be 30.00
    And the deposit paid should be 30.00
    And the order should list 1 allergen substitution
    And the kitchen confirms the substitution is nut-free
