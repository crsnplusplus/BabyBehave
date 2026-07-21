Feature: Priority patron handling in library request queue

  Background:
    Given a library patron management system

  @vip
  Scenario: VIP patron gets expedited processing
    Given a patron requests a book
    When the request is processed
    Then the patron should receive expedited service

  @urgent @overdue
  Scenario: Urgent overdue notice triggers priority handling
    Given a patron requests a book
    When the request is processed
    Then the patron should receive expedited service

  Scenario: Regular patron gets standard processing
    Given a patron requests a book
    When the request is processed
    Then the patron should receive standard service
