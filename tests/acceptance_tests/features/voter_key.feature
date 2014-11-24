Feature: Delegate a balance's voting power to a different address.
    Background:
        Given I'm Alice
        And I received 1000 XTS from angel
        And I made an account NewDelegate
        And I made an address Address1 for NewDelegate
        And I wait for one block
        And I transfer 100 XTS to address: Address1
        And I wait for one block
        And I get a balance ID for address: Address1 as B1

        And I made a wallet VoterWallet
        And I made an account Voter
        And I made an account Candidate1
        And I made an account Candidate2
        And I made an address VoterAddress for Voter

        And I switch to wallet default
        And I made a slate with delegate Candidate1 with id S1
        And I made a slate with delegate Candidate2 with id S2
        And I wait for one block

    Scenario: Update voter address, verify that vote updates with voter update, update voter address again, verify that it fails if old voter tries.
        When I set voter for balance B1 as Voter and save balance as B2
        And I wait for one block
        And switch to wallet VoterWallet
        And I update vote for balance B2
