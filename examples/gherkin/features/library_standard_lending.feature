Feature: Library standard lending

  Scenario: Checking out and returning a book on time
    Given the library has a copy of "The Hobbit"
    And Alice is a library member in good standing
    When Alice checks out "The Hobbit"
    Then the book should be checked out to Alice
    When Alice returns the book after 10 days
    Then the book should be available
    And Alice should not owe any fine
