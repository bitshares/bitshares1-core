Feature: Delegate a balance's voting power to a different address.
    Background:
        Given I'm Alice
        And I received 10000 XTS from angel
        And I made an account newdelegate
        And I made an address Address1 for newdelegate
        And I wait for one block
        And I register an account newdelegate as a delegate
        And I transfer 100 XTS to address: Address1
        And I wait for one block
        And I get balance ID for address: Address1 as B1

        Given I'm Bob
        And I made an account voter
        And I made an address VoterAddress for voter
        And I wait for one block

    Scenario: You can set the voter key and use it to vote
        When I'm Alice
        And I set voter for balance B1 as VoterAddress
        And I wait for 1 blocks
        And I get balance ID for address: Address1 as B2

        When I'm Bob
        And I approve newdelegate
        And I update vote for balance B2
        And I wait for one block
        And I get balance ID for address: Address1 as B3
        #        And I do transfer 50 XTS from address: Address1 to account: alice       TODO this should fail
        And I wait for one block
        Then votes for newdelegate should be positive
