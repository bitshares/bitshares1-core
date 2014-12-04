When(/^I make an object (\w+) with user data (\w+)$/) do |obj, data|
    @objects ||= {}
    current_objects = @current_actor.node.exec 'wallet_object_list', @current_actor.account
    puts current_objects
    @current_actor.node.exec 'wallet_object_create', @current_actor.account, '{"data":0}'
    new_objects = @current_actor.node.exec 'wallet_object_list', @current_actor.account
    puts new_objects
end

Then(/^Object with ID (\w+) should have user data (\w+)$/) do |arg1|
      pending # express the regexp above with the code you wish you had
end

When(/^I make an edge from (\w+) to (\w+) with name (\w+) and value (\w+)$/) do |arg1, arg2|
      pending # express the regexp above with the code you wish you had
end
