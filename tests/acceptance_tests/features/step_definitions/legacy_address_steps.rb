Given(/^I made a bitcoin address (\w+) for (\w+)$/) do |addr, acct|
    @addresses ||= {}
    @addresses[addr] = @current_actor.node.exec 'wallet_address_create', acct, "", 0
end

When(/^I transfer (\d+) (\w+) to legacy address (\w+)$/) do |amount, symbol, address|
    @current_actor.node.exec 'wallet_transfer_to_legacy_address', amount, symbol, @current_actor.account, @addresses[address]
end
