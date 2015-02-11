Feature: Cedric uses user-issued assets to make the MUSIC snapshot liquid
    Background:
        Given I'm Alice
        And I received 10000 XTS from angel
        And I wait for one block

    Scenario: Cedric distributes the snapshot using issue-to-many, lets people trade, then freezes trading again to take a snapshot for the real thing
        When I'm Alice
        And I create asset TESTNOTE (max shares: 1,000,000)
        And I wait for one block
        And I issue 100,000 TESTNOTE to myself
        And I wait for one block

        When I'm Bob
        And I made an account guy1
        And I made an address Guy1Addr for guy1
        And I made an account guy2
        And I made an address Guy2Addr for guy2

        When I'm Alice
        And I issue 100 TESTNOTE satoshis each to: [Guy1Addr, Guy2Addr]
        And I wait for one block

        Then Balance with owner Guy1Addr should have 100 TESTNOTE
        And Balance with owner Guy1Addr should have 100 TESTNOTE
