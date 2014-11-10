require 'rspec/expectations'
require 'logger'
require './testnet.rb'

Actor = Struct.new(:node, :account) do
end

class Helper

  def initialize
    @logger = Logger.new('features.log')
    @logger.info '--------------------------------------'
    @pause = false
  end

  def get_actor(name)
    if name == 'my' or name == 'I'
      @current_actor
    elsif name == 'Alice' or name == "Alice's"
      @alice
    elsif name == 'Bob' or name == "Bob's"
      @bob
    else
      raise "Unknown actor '#{name}'"
    end
  end

  def get_asset_by_name(currency)
    return @testnet.delegate_node.exec 'blockchain_get_asset', currency
  end

  def get_balance(data, account, currency)
    asset = get_asset_by_name(currency)
    asset_id = asset['id']
    data.each do |account_pair|
      if account_pair[0] == account
        account_pair[1].each do |balance_pair|
          next if balance_pair[0] != asset_id
          return balance_pair[1].to_f / asset['precision'].to_f
        end
      end
    end
    return 0
  end

  def exist_order(orders, o)
    #puts 'exist_order'
    #puts "order: #{o.inspect}"
    #puts "orders: #{orders.inspect}"
    @last_order_id = nil
    orders.each do |e|
      order = e[1]
      if order['type'] == o['Type'] and
        (o['Collateral'] and order['collateral'].to_f/100000.0 == o['Collateral'].to_f) and
        (o['Interest Rate'] and order['interest_rate']['ratio'].to_f * 100.0 == o['Interest Rate'].to_f)
        @last_order_id = e[0]
        return true
      end
    end
    return false
  end

  def exist_order_type(orders, type)
    #puts 'find_first_order_of_type'
    #puts "orders: #{orders.inspect}"
    @last_order_id = nil
    orders.each do |e|
      order = e[1]
      if order['type'] == type
        return true
      end
    end
    return false
  end

  def to_f(n)
    n.gsub(',','').to_f
  end

end

Before('@pause') do
  @pause = true
end

Before do |scenario|
  STDOUT.puts 'launching testnet, please wait..'
  @testnet = BitShares::TestNet.new(@logger)
  @testnet.create
  @testnet.alice_node.exec 'wallet_account_create', 'alice'
  @testnet.alice_node.exec 'wallet_account_register', 'alice', 'angel'
  @testnet.bob_node.exec 'wallet_account_create', 'bob'
  @testnet.bob_node.exec 'wallet_account_register', 'bob', 'angel'
  @testnet.alice_node.wait_new_block
  @alice = Actor.new(@testnet.alice_node, 'alice')
  @bob = Actor.new(@testnet.bob_node, 'bob')
  sleep(2)
end

After do |scenario|
  if @pause
    STDOUT.puts '@pause: use the following urls to access the nodes:'
    STDOUT.puts "delegate node: #{@testnet.delegate_node.url}"
    STDOUT.puts "alice node: #{@testnet.alice_node.url}"
    STDOUT.puts "bob node: #{@testnet.bob_node.url}"
    STDOUT.puts 'press any key to shutdown testnet and continue..'
    STDIN.getc
  end
  @pause = false
  STDOUT.puts 'shutting down testnet..'
  @testnet.shutdown
  @testnet = nil
end

World( RSpec::Matchers )
World{ Helper.new }
