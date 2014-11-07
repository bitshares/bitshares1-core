Given(/^I'm (\w+)$/) do |name|
  @current_actor = get_actor(name)
end

Given(/^(\w+) received ([\d,\.]+) ([A-Z]+) from angel/) do |name, amount, currency|
  actor = get_actor(name)
  actor.node.exec 'wallet_transfer', to_f(amount), currency, 'angel', actor.account
end

When(/^I send ([\d,\.]+) ([A-Z]+) to (\w+)$/) do |amount, currency, account|
  @current_actor.node.exec 'wallet_transfer', to_f(amount), currency, @current_actor.account, account.downcase
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
    amount = amount - multiplier * 0.5
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