Feature: Delegate a balance's voting power to a different address.
    Background:
        Given I'm Alice
        And I received 1000 XTS from angel
        And I made an account newdelegate
        And I made an address Address1 for newdelegate
        And I wait for one block
        And I transfer 100 XTS to address: Address1
        And I wait for one block
        And I get balance ID for address: Address1 as B1

        And I made a wallet voterwallet
        And I made an account voter
        And I made an address VoterAddress for voter

        And I switch to wallet default
        And I wait for one block

    Scenario: You can set the voter key and use it to vote
        When I set voter for balance B1 as VoterAddress
        And I wait for one block
        And I get balance ID for address: Address1 as B2
        And I switch to wallet voterwallet
        And I approve newdelegate
        And I update vote for balance B2
        And I wait for one block
        And I get balance ID for addresS: Address1 as B3
        Then votes for newdelegate should be positive
