Feature: You can use BTC addresses for a BTS balance.
    Background:
        Given I'm Alice
        And I received 1000 XTS from angel
        And I made an account otheracct
        And I made a bitcoin address addr1 for otheracct
        And I wait for one block


    Scenario: Deposit to and withdraw from a BTC address
        When I transfer 100 XTS to legacy address addr1
        And I wait for one block
        Then Balance with owner addr1 should have 100 XTS
        When I do transfer 99 XTS from legacy address: addr1 to public account for: otheracct
        And I wait for one block
        Then Balance with public accountname: otheracct should have 99 XTS

