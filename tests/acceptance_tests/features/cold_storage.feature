Feature: Deposit and withdraw to and from addresses using a transaction builder (pretending like it's over an airgap). No offline titan yet. These are rought the steps you would take to go from titan name in and out of cold storage address.


    Background:
        Given I'm Alice
        And I made an account onlineaccount
        And I made an address onlineaddress for onlineaccount
        And account onlineaccount received 1000 XTS from angel
        And I wait for one block
        And I made a wallet offlinewallet
        And I made an account offlineaccount
        And I made an address coldstorage for offlineaccount


    Scenario: Transfer from titan name to cold storate address
        When I switch to wallet default
        And I do transfer 10 XTS from accountname: onlineaccount to address: coldstorage
        And I wait for one block
        Then Balance with owner coldstorage should have 10 XTS

    Scenario: Transfer from address to TITAN public address, in case you only remember the name

        When I switch to wallet default
        And I made an account newaccount
        And I do transfer 10 XTS from accountname: onlineaccount to address: onlineaddress
        And I wait for one block
        Then Balance with owner onlineaddress should have 10 XTS

        When I do transfer 9 XTS from address: onlineaddress to public account for: newaccount
        And I wait for one block
        Then Balance with public accountname: newaccount should have 9 XTS

    Scenario: Transfer from cold storage to another address with an airgap. It fails unless I get a signature from the offline wallet!
        When I switch to wallet default
        And I do transfer 10 XTS from accountname: onlineaccount to address: coldstorage

        And I wait for one block
        Then Balance with owner coldstorage should have 10 XTS

        When I do offline transfer 9 XTS from address: coldstorage to address: onlineaddress as builder
        And I add signature and broadcast builder
        And I wait for one block
        Then Balance with owner coldstorage should have 10 XTS 

        When I switch to wallet offlinewallet
        And I add signature and broadcast builder
        And I wait for one block
        Then Balance with owner onlineaddress should have 9 XTS



        And I'm a happy shareholder!


