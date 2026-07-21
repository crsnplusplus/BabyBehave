Feature: Bulk order itemization

  Scenario: Wedding order with several item lines
    Given a bulk order for a wedding ceremony
    When the order contains the following items:
      | item              | quantity | unit_price |
      | wedding cake tier  | 3        | 50.00      |
      | cupcake dozen      | 5        | 22.00      |
      | macaron box        | 2        | 55.50      |
    Then the order total should be 371.00
    And the order should have 3 line items
