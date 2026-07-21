Feature: Book review submission

  Scenario: Patron submits a review with a multi-line body
    Given the library has a copy of "The Old Man and the Sea"
    When a patron Alice submits the following review:
      """
      A short, powerful story about persistence and the human spirit.

      Hemingway's masterful prose captures the profound struggle of an
      aging fisherman against both nature and his own limitations. The
      sparse dialogue and rich internal monologue create an intimate
      portrait of determination.

      Highly recommended for anyone who enjoys character-driven fiction
      and philosophical reflection on life's meaning.
      """
    Then the review should be visible with at least 50 words
    And the review text should contain "persistence"
