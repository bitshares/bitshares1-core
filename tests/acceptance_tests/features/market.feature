Feature: Short/cover BitUSD
  Being bullish on XTS
  I want to short BitUSD
  So I can profit from USD price drop

Scenario: Alice shorts BitUSD and sells to Bob, then Bob shorts and Alice covers
  Given I'm Alice
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
  And I should have the following USD/XTS market order:
    | Type        | Collateral | Interest Rate |
    | cover_order | 30000      | 10            |
  When Bob shorts USD, collateral 40,000 XTS, interest rate 11%
  And I submit ask for 12000 XTS at 0.01 USD/XTS
  And I wait for 2 blocks
  Then I should have 68,000 XTS minus 2*fee
  And I should have 120 USD
  And Bob should have 50,000 XTS minus 2*fee
  When I cover last USD margin order in full
  And I wait for 2 blocks
  Then I should have around 20 USD
  And I should have no USD/XTS margin orders

#@current @pause
Scenario: Alice shorts BitUSD with price limit, price feed moves short executes, Bob is able to sell 10 USD paying fee in USD
  Given I'm Alice
  And feed price is 0.02 USD/XTS
  And I wait for one block
  And I received 100,000 XTS from angel
  And Bob received 40,001 XTS from angel
  And I wait for one block
  When I short USD, collateral 10,000 XTS, interest rate 10%, price limit 0.01 USD/XTS
  And Bob submits ask for 20,000 XTS at 0.02 USD/XTS
  And I wait for one block
  Then Bob should have 0 USD
  And I should have no USD/XTS margin orders
  When feed price is 0.01 USD/XTS
  And I wait for one block
  And Bob submits ask for 20,000 XTS at 0.01 USD/XTS
  And I wait for 2 blocks
  Then Bob should have 50 USD and 0 XTS
  And I should have the following USD/XTS market order:
    | Type        | Collateral | Interest Rate |
    | cover_order | 15000      | 10            |
  When Bob submits bid for 1000 XTS at 0.01 USD/XTS
  And Bob waits for 2 blocks
  Then Bob should have around 50 USD and 1,000 XTS
