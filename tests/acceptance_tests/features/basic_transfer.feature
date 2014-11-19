Feature: Transfer funds from one account to another
  As a regular user
  I want to send funds to my friend
  So that I can make him happy

Scenario: Alice sends 100 XTS to Bob
  Given I'm Alice
  And I expect HTTP transaction callbacks
  And I received 1000 XTS from angel
  And I wait for one block
  And I print XTS balance
  When I send 100 XTS to Bob
  And I wait for one block
  Then I should have 900 XTS minus fee
  And Bob should have 100 XTS
  When Bob sends 50 XTS to me
  And I wait for one block
  Then I should have 950 XTS minus fee
  And transaction callback should match last transfer

Scenario: Bob sends 50 XTS to Alice
  Given I'm Bob
  And I received 500 XTS from angel
  And I wait for one block
  When I send 50 XTS to Alice
  And I wait for one block
  Then Alice should have 50 XTS
  And I should have 450 XTS minus fee
