Feature: Advanced User Issued Asset Functionality
    User issued assets can be used for many services.

    Background: AA
        Given I'm Bob
        And I received 1000 XTS from angel

        Given I'm Alice
        And I received 1000000 XTS from angel
        And I made an address authorityaddr for alice
        And I made an account customer
        And I wait for one block
        And I create asset ALICE (max shares: 1000000)
        And I register an account customer
        And I wait for one block
        And I give default auth permissions for ALICE to 1 of [authorityaddr]
        And I wait for one block


    Scenario: authority can still issue shares because hasn't set "restricted" flag.
        When I'm Alice
        And I issue 100 ALICE to account: customer
        And I wait for one block
        Then I should see the following assets:
      | Symbol  | Name       | Current Share Supply | Maximum Share Supply | Precision |
      | ALICE   | alice_iou  | 100000               | 1000000000           | 1000      |
    Scenario: Alice removes the "supply_unlimit" permission and can't change it back
        When I'm Alice
        And I set asset permissions for ALICE to: [restricted, retractable, market_halt, balance_halt]
        And I wait for one block
#        And I issue 100 ALICE to account: alice       TODO assert fail
#        And I wait for one block
#        Then I should see the following assets:
#      | Symbol  | Name       | Current Share Supply | Maximum Share Supply | Precision |
#      | ALICE   | alice_iou  | 100000               | 1000000000           | 1000      |

#TODO   this should fail!!
        And I set asset permissions for ALICE to: [restricted, retractable, market_halt, balance_halt, supply_unlimit]
        And I wait for one block

    Scenario: Customer can send his asset to anyone and anybody can trade in the ALICE market.
    Scenario: Alice can make the asset restricted and whitelist only customer, meaning Bob can't receive or trade in ALICE markets
    Scenario: Alice can freeze the market, meaning not even bob can trade.
        When I'm Bob
        And I submit ask for 10 XTS at 1 ALICE/XTS
        When I'm Alice
        And I set asset state for ALICE to: [market_halt]
        And I wait for one block
        When I'm Bob
#        And I submit ask for 10 XTS at 1 ALICE/XTS     TODO this should fail
    Scenario: Alice can unfreeze the market.
    Scenario: Alice can spend Bob's funds.
    Scenario: Alice can unfreeze funds and bob can spend them again.
    Scenario: Alice can restrict issuer's ability to freeze funds again.

