Feature: Burn/Post to account wall
  As regular user I want to post a message to my friend's wall
  So everybody could see my friend has some reputation measured in burnt money

  Scenario: Alice burns/posts a message to Bob's wall
    Given I'm Alice
    And I've received 1,000,000 XTS from angel
    And I wait for 2 blocks
    When I publicly burn 1,000 XTS for Bob with message 'From Alice with love'
    And I wait for one block
    Then Bob should see the following wall message:
      | Message              | Amount   |
      | From Alice with love | 1000 XTS |
