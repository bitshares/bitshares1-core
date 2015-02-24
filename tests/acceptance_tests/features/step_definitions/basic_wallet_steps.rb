Given(/^exec: (\w+)$/) do |argstr|
    args = argstr.split(";")
    @current_actor.node.exec(*args)
end


Given(/^I'm (\w+)$/) do |name|
  @current_actor = get_actor(name)
  @env ||= {}
end

Given(/I made a wallet (\w+)$/) do |name|
    @current_actor.node.exec 'wallet_create', name, 'password'
end

Given(/I made an account (\w+)$/) do |name|
    @current_actor.node.exec 'wallet_account_create', name
end

Given(/I register an account (\w+)( as a delegate)?$/) do |name, del|
    if del
        @current_actor.node.exec 'wallet_account_register', name, @current_actor.account, "", 1
    else
        @current_actor.node.exec 'wallet_account_register', name, @current_actor.account
    end
end

Given(/I rename account (\w+) to (\w+)$/) do |old_name, new_name|
    res = @current_actor.node.exec 'wallet_rename_account', old_name, new_name
    STDOUT.puts res
end

Given(/I ma[d|k]e an address (\w+) for (\w+)$/) do |addrname, acct|
    addr = @current_actor.node.exec 'wallet_address_create', acct
    @addresses ||= {}
    @addresses[addrname] = addr
end

Given(/^account (\w+) received ([\d,\.]+) ([A-Z]+) from angel/) do |name, amount, currency|
  @current_actor.node.exec 'wallet_transfer', to_f(amount), currency, 'angel', name
end

Given(/^(['\w]+) received ([\d,\.]+) ([A-Z]+) from angel/) do |name, amount, currency|
  actor = get_actor(name)
  actor.node.exec 'wallet_transfer', to_f(amount), currency, 'angel', actor.account
end

When(/I switch to wallet (\w+)$/) do |name|
    @current_actor.node.exec 'wallet_open', name
    @current_actor.node.exec 'wallet_unlock', '999999', 'password'
end

When(/^(\w+) sends? ([\d,\.]+) ([A-Z]+) to (\w+)$/) do |from, amount, currency, to|
  actor_from = get_actor(from)
  actor_to = get_actor(to)
  res = actor_from.node.exec 'wallet_transfer', to_f(amount), currency, actor_from.account, actor_to.account
  @transfers << res['ledger_entries'].first
end

When(/^I transfer (\d+) (\w+) to (legacy )?address: (\w+)$/) do |amount, symbol, legacy, address|
    if legacy
        @current_actor.node.exec 'wallet_transfer_to_legacy_address', amount, symbol, @current_actor.account, @addresses[address]
    else
        @current_actor.node.exec 'wallet_transfer_to_address', amount, symbol, @current_actor.account, @addresses[address]
    end
end

When(/^(\w+) waits? for (one|\d+) blocks?$/) do |name, blocks|
  actor = get_actor(name)
  blocks = if blocks == 'one' then 1 else blocks.to_i end
  (1..blocks).each do
    actor.node.wait_new_block
  end
end

When(/^([\w]+) prints? ([A-Z]+) balance$/) do |name, symbol|
  actor = get_actor(name)
  data = @current_actor.node.exec 'wallet_account_balance', actor.account
  balance = get_balance(data, actor.account, symbol)
  puts "balance: #{balance} #{symbol}"
end

Then(/^([\w]+) should have\s?(around)? ([\d,\.]+) ([A-Z]+)\s?(minus fee|minus \d+\*fee)?$/) do |name, around, amount, currency, minus_fee|
  amount = to_f(amount)
  actor = get_actor(name)
  data = actor.node.exec 'wallet_account_balance', actor.account
  balance = get_balance(data, actor.account, currency)
  if minus_fee and minus_fee.length > 0
    m = /(\d+)\s?\*\s?fee/.match(minus_fee)
    multiplier = if m and m[1] then m[1].to_i else 1 end
    amount = amount - multiplier * get_asset_fee(currency)
  end
  if around and around.length > 0
    amount = amount.round
    balance = balance.to_f.round
  end
  expect(balance.to_f).to eq(amount)
end

Then(/^([\w]+) should have\s?(around)? ([\d,\.]+) ([A-Z]+) and ([\d,\.]+) ([A-Z]+)$/) do |name, around, amount1, currency1, amount2, currency2|
  step "#{name} should have #{around} #{amount1} #{currency1}"
  step "#{name} should have #{around} #{amount2} #{currency2}"
end

Given(/^I expect HTTP transaction callbacks$/) do
  config = @current_actor.node.get_config
  config['wallet_callback_url'] = 'http://localhost:23232/transactions/create'
  @current_actor.node.stop
  @current_actor.node.save_config(config)
  @current_actor.node.start
  @current_actor.node.exec 'open', 'default'
  @current_actor.node.exec 'unlock', '9999999', 'password'
  @webserver = WebServer.new
  @webserver.start
end

Given(/^I am a Light Wallet Server named (\w+)$/) do |name|
  config = @current_actor.node.get_config
  config['rpc']['enable'] = true
  config['faucet_account_name'] = name
  config['relay_account_name'] = name
  config['rpc']['encrypted_rpc_endpoint'] = "127.0.0.1:5656"
  config['rpc']['encrypted_rpc_wif_key'] = "5K9kAzDG3YPGQwD1GLHjuw3NaUQQLeJFiZ4GCVBWd3HKpb29UJi"
  config['light_relay_fee'] = 25000
  @current_actor.node.stop
  @current_actor.node.save_config(config)
  @current_actor.node.start
  @current_actor.node.exec 'open', 'default'
  @current_actor.node.exec 'unlock', '9999999', 'password'
end

Then(/^transaction callback should match last transfer$/) do
  expect(@webserver.requests.length).to be > 0
  request = JSON.parse(@webserver.requests.last)
  expect(request).to eq(@transfers.last)
end


When(/^I get balance ID for address: (\w+) as (\w+)$/) do |addr, bal|
    bals = @current_actor.node.exec 'blockchain_list_address_balances', @addresses[addr]
    puts "get_address_balances result:"
    puts bals
    @addresses[bal] = bals[0][0]
end
