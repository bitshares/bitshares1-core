Feature: Mail server features

    Background:
        Given I'm Alice
        And I run a mail server
        Given I'm Bob
        And I use mail server accounts: alice
        
        And I wait for one block
    
    Scenario: Send mail to another user
        Given I'm Alice
        When I send mail to bob
        Then bob should receive my message
