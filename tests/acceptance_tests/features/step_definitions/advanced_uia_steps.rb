default_permissions = ["restricted", "retractable","market_halt","balance_halt", "supply_unlimit"]
default_state = ["retractable"]
Given(/^I give default auth permissions for (\w+) to (\d+) of (\[.*\])$/) do |asset, m, addrs|
    addr_name_list = JSON.parse(addrs.gsub(/(\w+)/, '"\1"'))
    addr_list = []
    for name in addr_name_list
        addr_list << @addresses[name]
    end
    @current_actor.node.exec('wallet_asset_update', asset, nil, nil, nil, nil, nil, 0, default_state, default_permissions, "",  m, addr_list)
end

Then(/^I set asset permissions for (\w+) to: (\[.*?\])$/) do |symbol, perm_list|
    puts perm_list
    existing = @current_actor.node.exec 'blockchain_get_asset', symbol
    m = existing["authority"]["required"]
    owners = existing["authority"]["owners"]
    perms = JSON.parse(perm_list.gsub(/(\w+)/, '"\1"'))
    puts perms
    @current_actor.node.exec 'wallet_asset_update', symbol, nil, nil, nil, nil, nil, 0, [], perms, "", m, owners
end

Then(/^I set asset state for (\w+) to: (\[.*?\])$/) do |symbol, state_list|
    puts state_list
    existing = @current_actor.node.exec 'blockchain_get_asset', symbol
    m = existing["authority"]["required"]
    owners = existing["authority"]["owners"]
    state = JSON.parse(state_list.gsub(/(\w+)/, '"\1"'))
    puts state
    @current_actor.node.exec 'wallet_asset_update', symbol, nil, nil, nil, nil, nil, 0, state
end
