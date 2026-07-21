Feature: Daily oven lifecycle

  Scenario: Bake a batch of croissants
    Given the oven is ready
    When I bake croissants
    Then the croissants should be golden brown

  Scenario: Bake a batch of baguettes
    Given the oven is ready
    When I bake baguettes
    Then the baguettes should have a crisp crust

  Scenario: Bake a batch of cookies
    Given the oven is ready
    When I bake cookies
    Then the cookies should be evenly baked
