Feature: Mail gets deleted

    Background:
        Given I'm Alice
        And I run a mail server
        And I wait for one block

    Scenario: Send mail then delete it
        Given I'm Bob
        When I send mail to alice
        Then alice should receive my message
        Given I'm Alice
        Then I should delete my message