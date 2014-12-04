When(/^I set voter for balance (\w+) as (\w+)$/) do |bal1, addr|
    puts @addresses
    @current_actor.node.exec 'wallet_balance_set_vote_info', @addresses[bal1], @addresses[addr]
end

When(/^I update vote for balance (\w+)$/) do |balID|
    bal = @current_actor.node.exec 'blockchain_get_balance', @addresses[balID]
    puts bal
    @current_actor.node.exec 'wallet_balance_set_vote_info', @addresses[balID]
end

When(/^I approve (\w+)$/) do |acct|
    @current_actor.node.exec 'wallet_account_set_approval', acct, true
end

Then(/^votes for (\w+) should be positive$/) do |acct|
    info = @current_actor.node.exec 'blockchain_get_account', acct
    puts info
    raise "Vote wasn't updated!" unless info["delegate_info"]["votes_for"] > 0
end
