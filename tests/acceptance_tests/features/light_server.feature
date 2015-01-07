Feature: Create and run a Light Wallet Server named lightserver

    Background: Alice has a wallet with a funded account named lightserver
        Given I'm Alice
        And I made an account lightserver
        And I received .5 XTS from angel
        And account lightserver received 10000 XTS from angel
        And I wait for one block
        And I register an account lightserver
        And I wait for one block
    
    @lightserver @pause
    Scenario: Alice runs a light wallet server named lightserver
        Given I'm Alice
        And I am a Light Wallet Server named lightserver
