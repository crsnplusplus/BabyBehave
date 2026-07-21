Feature: Flaky oven sensor tolerance with retry

  @retry:3
  Scenario: Preheat check tolerates transient sensor glitch
    Given the oven is warming up
    When I read the oven temperature
    Then the temperature reading should be stable and within range
