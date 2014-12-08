Feature: Account creation, deletion, and registration features

    Background:
        Given I'm Bob
        And I received 10 XTS from angel
        And I wait for one block
    
    Scenario: An account is created locally but then registered by another user
        Given I'm Alice
        And I made an account goodname
        
        Given I'm Bob
        And I made an account goodname
        And I register an account goodname
        And I wait for one block
        
        Given I'm Alice
        And I rename account goodname to badname

