Feature: Deposit to and withdraw from multi-signature account

@current
    Scenario: 2-of-3 multisig withdrawal should fail with 1 signature and succeed with 2
        Given I'm Alice
        And I made a wallet wallet1
        And I made an account acct1
        And I made an address addr1 for acct1

        And I made a wallet wallet2
        And I made an account acct2
        And I made an address addr2 for acct2

        And I made a wallet wallet3
        And I made an account acct3
        And I made an address addr3 for acct3

        And I switch to wallet default
        And I received 1000 XTS from angel
        And I wait for one block

        When I make multisig deposit 100 XTS from me to 2 of [addr1, addr2, addr3]
        And I save multisig ID for 2 of [addr1, addr2, addr3] as multi1
        And I wait for one block
        Then Balance with ID multi1 should have 100 XTS
        
        When I switch to wallet wallet1
        And I make an address addr4 for acct1
        And I start multisig withdrawal of 100 XTS minus fee from multi1 to addr4 as builder1
        And I add signature and broadcast builder1 expecting failure
        And I wait for one block
        Then Balance with ID multi1 should have 100 XTS

        When I switch to wallet wallet2
        And I add signature and broadcast builder1 expecting success
        And I wait for one block
        Then Balance with ID multi1 should have 0 XTS
