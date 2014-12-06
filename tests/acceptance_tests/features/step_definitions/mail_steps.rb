Given /^I run a mail server/  do
    account = @current_actor.account
    rpc_port = @testnet.mail_node.options[:rpc_port]
    pay_from = 'angel'
    result = @current_actor.node.exec 'blockchain_get_account', account
    public_data = result["public_data"] || {}
    public_data["mail_server_endpoint"] = "127.0.0.1:#{rpc_port}"
    public_data["mail_servers"] = [account] #also, uses own mail server
    result = @current_actor.node.exec\
        'wallet_account_update_registration', account, pay_from, public_data
    #puts result
    @testnet.mail_node.start # just to create the config file
    @testnet.wait_nodes [@testnet.mail_node]
    @testnet.mail_node.stop
    config = @testnet.mail_node.get_config
    config['mail_server_enabled'] = true
    @testnet.mail_node.save_config(config)
    @testnet.mail_node.start
end

Given /^I use mail server accounts: (.*)/  do |accounts|
    account = @current_actor.account
    pay_from = 'angel'
    new_mail_servers = accounts.scan /\w+/
    result = @current_actor.node.exec 'blockchain_get_account', account
    public_data = result["public_data"] || {}
    mail_servers = public_data["mail_servers"]
    mail_servers = public_data["mail_servers"] = [] unless mail_servers
    mail_servers.push *new_mail_servers
    result = @current_actor.node.exec\
        'wallet_account_update_registration', account, pay_from, public_data
    #puts result
end

When /I send mail to (\w+)$/ do |name|
    account = @current_actor.account
    mail_id = @current_actor.node.exec 'mail_send', account, name, 'mail_steps test subject', 'body'
    loop do
        msg = @current_actor.node.exec 'mail_get_processing_messages'
        #puts msg
        break unless msg[0] # sent
        status = msg[0][0]
        if status == "failed"
            message = @current_actor.node.exec 'mail_get_message', mail_id
            if message["failure_reason"]
                puts message
                raise message["failure_reason"]
            end
        end
        break unless status == "transmitting" or status == "proof_of_work"
        sleep 0.2
    end
    
end

Then /(\w+) should receive my message/ do |name|
    actor = get_actor(name)
    mail_check_new_messages = actor.node.exec 'mail_check_new_messages'
    raise 'Expecting 1 message, instead got #{mail_check_new_messages}' unless mail_check_new_messages == 1
    #puts 'mail_check_new_messages', mail_check_new_messages
    mail_inbox = actor.node.exec 'mail_inbox'
    #puts 'mail_inbox',mail_inbox
    raise 'Expecting 1 message, instead got #{mail_inbox}' unless mail_inbox and mail_inbox.length == 1
end