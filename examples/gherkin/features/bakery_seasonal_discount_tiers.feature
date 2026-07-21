Feature: Bakery seasonal discount tiers for loyalty customers

  Scenario Outline: Customer loyalty tier receives appropriate discount
    Given a new custom cake order
    And the customer has loyalty tier "<tier_name>"
    And the cake is a "seasonal" cake serving <servings> guests
    When the order total is <order_total>
    Then the discount percentage should be <discount_percentage>
    And the final price should be <final_price>

    Examples:
      | tier_name | servings | order_total | discount_percentage | final_price |
      | bronze    | 5        | 12.50       | 0                   | 12.50       |
      | silver    | 10       | 25.00       | 5                   | 23.75       |
      | gold      | 15       | 37.50       | 10                  | 33.75       |
      | platinum  | 20       | 50.00       | 15                  | 42.50       |
