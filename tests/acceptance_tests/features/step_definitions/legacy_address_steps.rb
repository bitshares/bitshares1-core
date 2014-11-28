Given(/^I made a bitcoin address (\w+) for (\w+)$/) do |addr, acct|
    @addresses ||= {}
    @addresses[addr] = @current_actor.node.exec 'wallet_address_create', acct, "", 0
end


