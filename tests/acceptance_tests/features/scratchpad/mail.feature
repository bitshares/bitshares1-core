Feature: Mail server features

    Background:
        Given I'm Alice
        And I run a mail server
        
        Given I'm Bob
        And I use mail server accounts: alice
    
    Scenario: Send mail to another user
        Given I'm Bob
        When I send mail to alice
        Then alice should receive my message
