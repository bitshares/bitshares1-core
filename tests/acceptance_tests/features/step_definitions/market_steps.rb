Given(/^([A-Z]+)\/([A-Z]+) market$/) do |symbol1, symbol2|
  @market_asset1 = get_asset_by_name(symbol1)
  @market_asset2 = get_asset_by_name(symbol2)
end

Given(/^feed price is (.+) ([A-Z]+)\/XTS$/) do |price, symbol|
  @feed_price = price.to_f
  node = @testnet.delegate_node
  @testnet.for_each_delegate do |delegate_name|
    node.exec 'wallet_publish_price_feed', delegate_name, price, symbol
  end
end

Given(/^delegates publish price feeds:$/) do |table|
  feeds = table.hashes.map {|f| [f['Symbol'], f['Price'].to_f]}
  node = @testnet.delegate_node
  @testnet.for_each_delegate do |delegate_name|
    node.exec 'execute_command_line', "wallet_publish_feeds #{delegate_name} #{feeds.to_s}"
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
  amount = to_f(amount)
  if order_type == 'bid'
    actor.node.exec 'wallet_market_submit_bid', actor.account, amount, symbol, price, ps1
  elsif order_type == 'ask'
    actor.node.exec 'wallet_market_submit_ask', actor.account, amount, symbol, price, ps1
  else
    raise "Unknown order type: #{order_type}"
  end
end

#market_submit_relative_ask: (from_account_name, sell_quantity, sell_quantity_symbol, relative_ask_price, ask_price_symbol, limit_ask_price, error_handler = null) ->
When(/^(\w+) submits? relative (bid|ask) for ([\d,\.]+) ([A-Z]+) at ([\d\.]+) ([A-Z]+)\/([A-Z]+) (above|below) feed price$/) do |name, order_type, amount, symbol, relative_price, ps1, ps2, above_or_below|
  actor = get_actor(name)
  amount = to_f(amount)
  if order_type == 'bid'
    actor.node.exec 'wallet_market_submit_relative_bid', actor.account, amount, symbol, relative_price, ps1
  elsif order_type == 'ask'
    actor.node.exec 'wallet_market_submit_relative_ask', actor.account, amount, symbol, relative_price, ps1
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

Then(/^(\w+) should have the following market orders?:$/) do |name, orders_table|
  actor = get_actor(name)
  orders_table.hashes.each do |o|
    orders = actor.node.exec 'wallet_market_order_list', @market_asset1['symbol'], @market_asset2['symbol'], -1
    found = exist_order(orders, o)
    raise "Order not found: #{o} in #{orders}" unless found
  end
end

Then(/^(\w+) should have no (\w+) orders$/) do |name, type|
  type = 'cover' if type == 'margin'
  actor = get_actor(name)
  orders = actor.node.exec 'wallet_market_order_list', @market_asset1['symbol'], @market_asset2['symbol'], -1
  found = exist_order_type(orders, type+'_order')
  raise 'Order exists!' if found
end

When(/^(\w+) cancels? (first|last|all) (bid|ask|short) orders$/) do |name, selector, order_type|
  order_type = order_type + '_order'
  actor = get_actor(name)
  orders = actor.node.exec 'wallet_market_order_list', @market_asset1['symbol'], @market_asset2['symbol'], -1
  raise 'No orders to cancel' if orders.empty?
  if selector == 'all'
    orders.each do |order|
      actor.node.exec('wallet_market_cancel_order', order[0]) if order[1]['type'] == order_type
    end
  elsif selector == 'first'
    actor.node.exec('wallet_market_cancel_order', orders.first[0]) if orders.first[1]['type'] == order_type
  elsif selector == 'last'
    actor.node.exec('wallet_market_cancel_order', orders.last[0]) if orders.last[1]['type'] == order_type
  end
end
