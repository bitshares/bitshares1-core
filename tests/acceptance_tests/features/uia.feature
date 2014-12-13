Feature: User Issued Assets
  As centralized bitcoin exchange owner
  I want to issue my own IOUs on BitShares exchange
  So I can profit from charging BitShares users a fee for depositing/withdrawing my IOUs


  Background: Alice creates asset and issues all shares to herself
    Given I'm Alice
    And I received 100,000,000 XTS from angel
    And I wait for 2 blocks
    When I create asset ALCBTC (max shares: 1,000,000)
    And I wait for one block
    When I issue 500,000 ALCBTC to myself
    And I wait for one block
    Then I should have 500,000 ALCBTC
    And Bob should see the following assets:
      | Symbol  | Name       | Current Share Supply | Maximum Share Supply | Precision |
      | ALCBTC  | alcbtc_iou | 500000000            | 1000000000           | 1000      |

  Scenario: Alice sells some of her shares to Bob for XTS; also Alice sends a gift to bob
    Given ALCBTC/XTS market
    And Bob received 10,000 XTS from angel
    And Bob waits for one block
    When Alice submits bid for 1,000 XTS at 2.0 ALCBTC/XTS
    And Bob submits ask for 1,000 XTS at 2.0 ALCBTC/XTS
    And Bob waits for 2 blocks
    Then Alice should have 498,000 ALCBTC
    And Bob should have 2,000 ALCBTC
    And Bob should have 9,000 XTS minus fee
    When Alice sends 100,000 ALCBTC to Bob
    And Bob waits for one block
    Then Bob should have 102,000 ALCBTC

 Scenario: Alice sells some of her shares to Bob for BitUSD
    Given feed price is 0.01 USD/XTS
    And I wait for one block
    And Bob received 100,000 XTS from angel
    And I wait for 2 blocks
    When I short USD, collateral 20,000 XTS, interest rate 10%
    And Bob submits ask for 10,000 XTS at 0.01 USD/XTS
    And I wait for 2 blocks
    Then Bob should have 100 USD
    When Alice submits bid for 10 USD at 200 ALCBTC/USD
    And Bob submits ask for 10 USD at 200 ALCBTC/USD
    And Bob waits for 2 blocks
    Then Alice should have 498,000 ALCBTC
    And Bob should have 2,000 ALCBTC
    And Bob should have around 90 USD minus fee


#  on ramp scenario (not intended to run) :
#    As a regular user I want to convert real BTC to BitBTC
#    1. I create Bitstamp account
#    2. I deposit BTC to Bitstamp
#    3. I buy BitstampBTC
#    4. I withdraw BitstampBTC to my BitShares account
#    5. I trade BitstampBTC to BitBTC
