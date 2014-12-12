When(/^I make an object: (\w+)  with user data: (\w+)$/) do |obj, data|
    @objects ||= {}
    current_objects = @current_actor.node.exec 'wallet_object_list', @current_actor.account
    @current_actor.node.exec 'wallet_object_create', @current_actor.account, data
    @current_actor.node.wait_new_block
    new_objects = @current_actor.node.exec 'wallet_object_list', @current_actor.account
    puts current_objects
    puts new_objects

    new_objects.each do |new|
        found = false

        current_objects.each do |old|
            if new["_id"] == old["_id"]
                found = true
            end
        end
        if not found
            @objects[obj] = new
        end
    end
    puts @objects
end

Then(/^Object with ID: (\w+)  should have user data: (\w+)$/) do |obj, data|
    id = @objects[obj]["_id"]
    objects = @current_actor.node.exec 'wallet_object_list', @current_actor.account
    puts objects
    object = {}
    objects.each do |o|
        if o["_id"] == id
            object = o
        end
    end
    raise "Wrong user data!" unless object["user_data"] == data
end

When(/^I make an edge from (\w+) to (\w+) with name (\w+) and value (\w+)$/) do |from, to, name, value|
    @current_actor.node.exec 'wallet_set_edge', @current_actor.account, @objects[from]["_id"], @objects[to]["_id"], name, value
end

Then(/^Edge from (\w+) to (\w+) named (\w+) should have value (\w+)$/) do |from, to, name, value|
    puts @objects
    edges = @current_actor.node.exec 'blockchain_get_edges', @objects[from]["_id"], @objects[to]["_id"], name
    puts edges
    raise "Somehow there are multiple edge object for a unique id" unless edges.length <= 1
    raise "Edge has wrong value!" unless edges[0]["value"] == value
end
