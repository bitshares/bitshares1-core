Feature: Short/cover BitUSD
  Being bullish on XTS
  I want to short BitUSD
  So I can profit from USD price drop

@current @pause
Scenario: Alice shorts BitUSD and sells to Bob, then Bob shorts and Alice covers
  Given USD/XTS market
  And I'm Alice
  And feed price is 0.01 USD/XTS
  And I wait for one block
  And I received 100,000 XTS from angel
  And Bob received 100,000 XTS from angel
  And I wait for 2 blocks
  When I short USD, collateral 20,000 XTS, interest rate 10%
  And Bob submits ask for 10,000 XTS at 0.01 USD/XTS
  And I wait for 2 blocks
  Then Bob should have 100 USD
  And I should have 80,000 XTS minus fee
  And I should have the following market order:
    | Type        | Collateral | Interest Rate |
    | cover_order | 30000      | 10            |




Scenario: Alice shorts BitUSD with price limit, price feed moves short executes, Bob is able to sell 10 USD paying fee in USD
  Given USD/XTS market
  And I'm Alice
  And feed price is 0.02 USD/XTS
  And I wait for one block
  And I received 100,000 XTS from angel
  And Bob received 40,001 XTS from angel
  And I wait for one block
  When I short USD, collateral 10,000 XTS, interest rate 10%, price limit 0.01 USD/XTS
  And Bob submits ask for 20,000 XTS at 0.02 USD/XTS
  And I wait for one block
  Then Bob should have 0 USD
  And Bob should have the following market order:
    | Type        | Balance | Price |
    | ask_order   | 20000   | 0.02  |
  And I should have no margin orders
  When feed price is 0.01 USD/XTS
  And I wait for one block
  And Bob submits ask for 20,000 XTS at 0.01 USD/XTS
  And I wait for 2 blocks
  Then Bob should have 50 USD and 0 XTS
  And I should have the following market order:
    | Type        | Collateral | Interest Rate |
    | cover_order | 15000      | 10            |
  When Bob submits bid for 1000 XTS at 0.01 USD/XTS
  And Bob waits for 2 blocks
  Then Bob should have around 50 USD and 1,000 XTS
  When Bob cancels all ask orders
  And Bob waits for 1 block
  Then Bob should have around 50 USD and 34,999.0 XTS


Scenario: Alice shorts BitUSD and sells to Bob, and later Alice uses relative order to buy back Bob's BitUSD
  Given I'm Alice
  And USD/XTS market
  And feed price is 0.01 USD/XTS
  And I wait for one block
  And I received 100,000 XTS from angel
  And Bob received 100,000 XTS from angel
  And I wait for 2 blocks
  When I short USD, collateral 20,000 XTS, interest rate 10%
  And Bob submits ask for 10,000 XTS at 0.01 USD/XTS
  And I wait for 2 blocks
  Then Bob should have 100 USD
  And I should have 80,000 XTS minus fee

  And I submit ask for 10,000 XTS at 0.012 USD/XTS
  And I wait for 1 block
  And Bob submits relative bid for 8,000 XTS at 0.001 USD/XTS above feed price
  And Bob should have the following market order:
    | Type        |  |  |
    | bid_order   |  |  |
  #  And Bob submits bid for 8,000 XTS at 0.012 USD/XTS
  #  And I should have the following USD/XTS market order:
  #    | Type        | Collateral | Interest Rate |
  #    | cover_order | 30000      | 10            |
  #  When Bob shorts USD, collateral 40,000 XTS, interest rate 11%
  #  And I submit ask for 12000 XTS at 0.01 USD/XTS
  #  And I wait for 2 blocks
  #  Then I should have 68,000 XTS minus 2*fee
  #  And I should have 120 USD
  #  And Bob should have 50,000 XTS minus 2*fee
  #  When I cover last USD margin order in full
  #  And I wait for 2 blocks
  #  Then I should have around 20 USD
  #  And I should have no USD/XTS margin orders

Scenario: Alice shorts BitUSD, Bob shorts BitBTC, then they can trade USD vs BTC
  Given I'm Alice
  And delegates publish price feeds:
    | Symbol | Price |
    | USD    | 0.01  |
    | BTC    | 0.005 |
  And I wait for one block
  And I received 100,000 XTS from angel
  And Bob received 100,000 XTS from angel
  And I wait for 2 blocks
  When I short USD, collateral 20,000 XTS, interest rate 10%
  And Bob submits ask for 10,000 XTS at 0.01 USD/XTS
  And I wait for 2 blocks
  Then Bob should have 100 USD
  When Bob shorts BTC, collateral 20,000 XTS, interest rate 2%
  And I submit ask for 10,000 XTS at 0.005 BTC/XTS
  And I wait for 2 blocks
  Then I should have 50 BTC
  Given USD/BTC market
  And I submit ask for 3 BTC at 4.0 USD/BTC
  And I wait for one block
  And Bob submits bid for 5 BTC at 4.0 USD/BTC
  And I wait for 2 blocks
  Then I should have around 47 BTC and 12 USD
  And Bob should have the following market order:
    | Type        | Balance | Price |
    | bid_order   | 8       | 4     |
