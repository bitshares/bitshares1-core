Feature: You can make objects with generic data and edges between objects.
    Background:
        Given I'm Alice
        And I received 1000 XTS from angel
        #        And I made a wallet otherwallet
        #And I switch to wallet default
        And I wait for one block

    Scenario: I can make an edge between basic objects!
        When I make an object: obj1  with user data: MyUserDataString
        And I make an object: obj2  with user data: OtherUserDataString
        And I wait for one block
        And I make an edge from obj1 to obj2 with name Trust and value true
        And I wait for one block
        Then Object with ID: obj1  should have user data: MyUserDataString
        And Edge from obj1 to obj2 named Trust should have value true
        #Scenario: I can make an edge from an account!
        #Scenario: I can make an edge from an asset!
        #Scenario: Once I transfer an object, only the new owner can update its edges!
