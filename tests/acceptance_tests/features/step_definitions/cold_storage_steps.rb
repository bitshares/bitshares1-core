When(/^I do transfer (\d+) (\w+) from accountname: (\w+) to address: (\w+)$/) do |amount, symbol, account, address|
    @current_actor.node.exec 'wallet_transfer_asset_to_address', amount, symbol, account, @addresses[address]
end

When(/^I do transfer (\d+) (\w+) from address: (\w+) to account: (\w+)$/) do |amount, symbol, address, account|
    @current_actor.node.exec 'wallet_withdraw_from_address', amount, symbol, @addresses[address], account
end
