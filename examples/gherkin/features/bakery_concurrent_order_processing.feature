Feature: Bakery concurrent order processing
  Multiple independent customer orders are processed in parallel,
  each with its own inventory and payment state. There is zero
  shared mutable state between orders - each order's scenario
  runs independently and concurrently without any locks, atomics,
  or synchronization.

  Scenario: Alice's chocolate cake order
    Given a new custom cake order
    And the cake is a "chocolate" cake serving 8 guests
    When a deposit of 10.00 is paid
    Then the total price should be 20.00
    And the deposit paid should be 10.00

  Scenario: Bob's vanilla cake with allergen substitution
    Given a new custom cake order
    And the cake is a "vanilla" cake serving 6 guests
    When the customer requests a "gluten" allergen substitution
    And a deposit of 15.00 is paid
    Then the total price should be 20.00
    And the deposit paid should be 15.00

  Scenario: Carol's red velvet cake
    Given a new custom cake order
    And the cake is a "red velvet" cake serving 12 guests
    When a deposit of 20.00 is paid
    Then the total price should be 30.00
    And the deposit paid should be 20.00

  Scenario: Diana's carrot cake with multiple allergen substitutions
    Given a new custom cake order
    And the cake is a "carrot" cake serving 4 guests
    When the customer requests a "dairy" allergen substitution
    And the customer requests a "nuts" allergen substitution
    And a deposit of 8.00 is paid
    Then the total price should be 20.00
    And the deposit paid should be 8.00
    And the order should list 2 allergen substitution
