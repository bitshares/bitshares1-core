require 'rspec/expectations'
require 'logger'
require './testnet.rb'
require './webserver.rb'

Actor = Struct.new(:node, :account) do
end

class Helper

  def initialize
    Dir.mkdir 'tmp' unless Dir.exist? 'tmp'
    FileUtils.rm_rf('tmp/lastrun.log')
    @logger = Logger.new('tmp/lastrun.log')
    @pause = false
  end

  def get_actor(name)
    name = name.downcase
    if name == 'my' or name == 'me' or name == 'i' or name == "i've" or name == 'mine' or name == 'myself'
      @current_actor
    elsif name == 'alice' or name == "alice's"
      @alice
    elsif name == 'bob' or name == "bob's"
      @bob
    else
      raise "Unknown actor '#{name}'"
    end
  end

  def get_asset_by_name(symbol)
    return @market_asset1 if @market_asset1 and @market_asset1['symbol'] == symbol
    return @market_asset2 if @market_asset2 and @market_asset2['symbol'] == symbol
    return @testnet.delegate_node.exec 'blockchain_get_asset', symbol
  end

  def get_asset_by_id(id)
    return @market_asset1 if @market_asset1 and @market_asset1['id'].to_i == id
    return @market_asset2 if @market_asset2 and @market_asset2['id'].to_i == id
    return @testnet.delegate_node.exec 'blockchain_get_asset', id
  end

  def get_asset_fee(symbol)
    fee = @testnet.delegate_node.exec 'wallet_get_transaction_fee', symbol
    asset = get_asset_by_id(fee['asset_id'])
    return fee['amount'].to_f / asset['precision'].to_f
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

  #{"type"=>"ask_order", "market_index"=>{"order_price"=>{"ratio"=>"0.002", "quote_asset_id"=>7, "base_asset_id"=>0}, "owner"=>"XTSJA72rtoSYfbWvEDm4zZKve5ucNqhKRrCP"}, "state"=>{"balance"=>2000000000, "limit_price"=>nil, "last_update"=>"2014-11-18T22:34:50"}, "collateral"=>nil, "interest_rate"=>nil, "expiration"=>nil}
  def parse_ask_bid_order(o)
    res = {}
    order_price = o['market_index']['order_price']
    ratio = order_price['ratio'].to_f
    quote_asset = get_asset_by_id(order_price['quote_asset_id'])
    base_asset = get_asset_by_id(order_price['base_asset_id'])
    res[:price] = ratio * (base_asset['precision'].to_f / quote_asset['precision'].to_f)
    balance_asset = o['type'] == 'ask_order' ? base_asset : quote_asset
    res[:balance] = o['state']['balance'].to_f / balance_asset['precision'].to_f
    return res
  end

  def exist_order(orders, o)
    # puts 'exist_order'
    # puts "order: #{o.inspect}"
    # puts "orders: #{orders.inspect}"
    @last_order_id = nil
    orders.each do |e|
      order = e[1]
      next unless order['type'] == o['Type']
      if order['type'] == 'cover_order'
        if order['collateral'].to_f/100000.0 == o['Collateral'].to_f and
           order['interest_rate']['ratio'].to_f * 100.0 == o['Interest Rate'].to_f
          @last_order_id = e[0]
          return true
        end
      end
      if order['type'] == 'ask_order' or order['type'] == 'bid_order'
        po = parse_ask_bid_order(order)
        if po[:balance] == o['Balance'].to_f and po[:price] == o['Price'].to_f
          @last_order_id = e[0]
          return true
        end
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

  def asset_amount_to_str(am)
    asset = @testnet.delegate_node.exec 'blockchain_get_asset', am['asset_id']
    return "#{am['amount'].to_f/asset['precision'].to_f} #{asset['symbol']}"
  end

  def print_json(json)
    json = JSON.parse(json) if json.class == String
    STDOUT.puts JSON.pretty_generate(json)
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
  @transfers = []
  sleep(2)
end

After do |scenario|
  @testnet.pause if @pause
  @pause = false
  STDOUT.puts 'shutting down testnet..'
  @testnet.shutdown
end

World( RSpec::Matchers )
World{ Helper.new }
