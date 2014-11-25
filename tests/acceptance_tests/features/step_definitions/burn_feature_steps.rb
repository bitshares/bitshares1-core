When(/^(\w+) (publicly|anonymously) burn ([\d,\.]+) ([A-Z]+) (for|against) (\w+) with message '(.+)'$/) do |from, anonymous, amount, currency, for_or_against, to, message|
  from_actor = get_actor(from)
  to_actor = get_actor(to)
  from_actor.node.exec 'wallet_burn', to_f(amount), currency, from_actor.account, for_or_against, to_actor.account, message, anonymous == 'anonymously'
end

Then(/^(\w+) should see the following wall messages?:$/) do |name, table|
  data = table.hashes.map { |r| r }
  actor = get_actor(name)
  messages = actor.node.exec 'blockchain_get_account_wall', actor.account
  data.each do |dm|
    match = false
    amount = dm['Amount'].split(' ')
    amount_str = "#{amount[0].to_f} #{amount[1]}"
    messages.each do |mm|
      next until dm['Message'] == mm['message']
      mm_amount = asset_amount_to_str(mm['amount'])
      match = true if amount_str == mm_amount
    end
    raise "#{dm} is not found in #{messages}" unless match
  end
end
