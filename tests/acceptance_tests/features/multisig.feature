Feature: Deposit to and withdraw from multi-signature account
    Background:
        Given I'm Alice
        And I made a wallet wallet1
        And I made an account account1
        And I made an address address1 for account1

        And I made a wallet wallet2
        And I made an account account2
        And I made an address address2 for account2

        And I made a wallet wallet3
        And I made an account account3
        And I made an address address3 for account3

        And I made a wallet wallet4
        And I made an account account4
        And I made an address address4 for account4


    Scenario: 2-of-3 multisig withdrawal should fail with 1 signature and succeed with 2

        When I switch to wallet default
        And I received 1000 XTS from angel
        And I wait for one block

        When I make multisig deposit 100 XTS from me to 2 of [address1, address2, address3]
        And I save multisig ID for XTS with 2 of [address1, address2, address3] as multi1
        And I wait for one block
        Then Balance with ID multi1 should have 100 XTS
        
        When I switch to wallet wallet1
        And I make an address normal_address for account1
        And I start multisig withdrawal of 99 XTS from multi1 to normal_address as builder1
        And I add signature and broadcast builder1
        And I wait for one block
        Then Balance with ID multi1 should have 100 XTS

        When I switch to wallet wallet2
        And I add signature and broadcast builder1
        And I wait for one block
        Then Balance with ID multi1 should have 0 XTS
        And Balance with owner normal_address should have 99 XTS


        And I'm a happy shareholder!


    Scenario: 2-of-3 multisig using "latest builder" feature

        When I switch to wallet default
        And I received 1000 XTS from angel
        And I wait for one block

        And I make multisig deposit 100 XTS from me to 2 of [address2, address3, address4]
        And I save multisig ID for XTS with 2 of [address2, address3, address4] as multi2
        And I wait for one block
        Then Balance with ID multi2 should have 100 XTS

        When I switch to wallet wallet2
        And I make an address normal_address for account2
        And I start multisig withdrawal of 99 XTS from multi2 to normal_address
        And I add signature and broadcast
        And I wait for one block
        Then Balance with ID multi2 should have 100 XTS

        When I switch to wallet wallet3
        And I add signature and broadcast
        And I wait for one block
        Then Balance with ID multi2 should have 0 XTS
        And Balance with owner normal_address should have 99 XTS


