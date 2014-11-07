Given(/^feed price is (.+) ([A-Z]+)\/XTS$/) do |price, symbol|
  @feed_price = price.to_f
  node = @testnet.delegate_node
  @testnet.for_each_delegate do |delegate_name|
    node.exec 'wallet_publish_price_feed', delegate_name, price, symbol
  end
end

When(/^(\w+) shorts? ([A-Z]+), collateral ([\d,\.]+) ([A-Z]+), interest rate ([\d\.]+)%, price limit ([\d\.]+) ([A-Z]+)\/([A-Z]+)$/) do |name, symbol, collateral, collateral_symbol, ir, price_limit, ps1, ps2|
  actor = get_actor(name)
  actor.node.exec 'wallet_market_submit_short', actor.account, to_f(collateral), collateral_symbol, ir, symbol, price_limit
end

When(/^(\w+) shorts? ([A-Z]+), collateral ([\d,\.]+) ([A-Z]+), interest rate ([\d\.]+)%$/) do |name, symbol, collateral, collateral_symbol, ir|
  step "#{name} short #{symbol}, collateral #{collateral} #{collateral_symbol}, interest rate #{ir}%, price limit 0.0 USD/XTS"
end

When(/^(\w+) submits? (bid|ask) for ([\d,\.]+) ([A-Z]+) at ([\d\.]+) ([A-Z]+)\/([A-Z]+)$/) do |name, order_type, amount, symbol, price, ps1, ps2|
  actor = get_actor(name)
  #actor.node.exec 'rescan'
  data = actor.node.exec 'wallet_account_balance', actor.account
  balance = get_balance(data, actor.account, symbol)
  #puts "#{actor.account}'s balance: #{balance} #{symbol}"
  amount = to_f(amount)
  if order_type == 'bid'
    actor.node.exec 'wallet_market_submit_bid', actor.account, amount, symbol, price, ps1
  elsif order_type == 'ask'
    actor.node.exec 'wallet_market_submit_ask', actor.account, amount, symbol, price, ps1
  else
    raise "Unknown order type: #{order_type}"
  end
end

When(/^I cover last ([A-Z]+) margin order in full$/) do |symbol|
  raise 'last order not defined' unless @last_order_id
  @current_actor.node.exec 'wallet_market_cover', @current_actor.account, 0, symbol, @last_order_id
end

Then(/^Bob's balance should increase by ([\d,\.]+) USD$/) do |arg1|
  pending # express the regexp above with the code you wish you had
end

Then(/^(\w+) should have the following ([A-Z]+)\/([A-Z]+) market orders?:$/) do |name, symbol1, symbol2, orders_table|
  actor = get_actor(name)
  orders_table.hashes.each do |o|
    orders = actor.node.exec 'wallet_market_order_list', symbol1, symbol2, 0
    found = exist_order(orders, o)
    raise "Order not found: #{o} in #{orders}" unless found
  end
end

Then(/^(\w+) should have no ([A-Z]+)\/([A-Z]+) (\w+) orders$/) do |name, symbol1, symbol2, type|
  type = 'cover' if type == 'margin'
  actor = get_actor(name)
  orders = actor.node.exec 'wallet_market_order_list', symbol1, symbol2, 0
  found = exist_order_type(orders, type+'_order')
  raise 'Order exists!' if found
end
