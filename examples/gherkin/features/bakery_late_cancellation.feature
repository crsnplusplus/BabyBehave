Feature: Bakery late cancellation forfeits the deposit

  Scenario: Cancelling two days before pickup forfeits the deposit
    Given a new custom cake order
    And the cake is a "red velvet" cake serving 12 guests
    When a deposit of 36.00 is paid
    And the order is cancelled 2 days before pickup
    Then the deposit should be forfeited
    But the deposit should be refunded
