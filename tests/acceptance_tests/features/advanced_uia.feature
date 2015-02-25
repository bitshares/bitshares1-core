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


    Scenario: authority can still issue shares because hasn't set "supply_unlimit" flag.
        When I'm Alice
        And I issue 100 ALICE to account: customer
        And I wait for one block
        Then I should see the following assets:
      | Symbol  | Name       | Current Share Supply | Maximum Share Supply | Precision |
      | ALICE   | alice_iou  | 100000               | 1000000000           | 1000      |

    Scenario: Alice can make the asset restricted and whitelist only customer, meaning Bob can't receive or trade in ALICE markets
        When I'm Alice
        And I authorize customer for ALICE asset
        And I traf
    Scenario: Alice can spend customer's funds.
    Scenario: Alice can freeze the market

