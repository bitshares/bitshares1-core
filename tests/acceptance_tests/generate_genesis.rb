#!/usr/bin/env ruby

require 'json'

raise 'BTS_BUILD env variable is not set' unless ENV['BTS_BUILD']
@create_key_binary = ENV['BTS_BUILD'] + '/programs/utils/bts_create_key'

def create_key
  keys = `#{@create_key_binary}`.split("\n")
  public_key = /: (\w+)/.match(keys[0])[1]
  private_key = /: (\w+)/.match(keys[2])[1]
  pts_key = /: (\w+)/.match(keys[4])[1]
  return public_key, private_key, pts_key
end

delegate_keys = []
balance_keys = []

for i in 0..100
  delegate_keys << create_key
end

for i in 0..3
  balance_keys << create_key
end

#puts balance_keys

genesis = JSON.parse( IO.read('genesis_base.json') )

genesis['names'].clear
genesis['balances'].clear

total_balance = 20000000000000
delegate_keys.each_with_index do |k, i|
  genesis['names'] << {"name" => "delegate#{i}", "delegate_pay_rate" => "100", "owner" => k[0]}
  genesis['balances'] << [k[2], 10000000000]
  total_balance -= 10000000000
end

balance_keys.each_with_index do |k, i|
  genesis['balances'] << [k[2], total_balance/4]
end

File.write('genesis.json', JSON.pretty_generate(genesis))
puts 'generated genesis.json'

File.open('genesis.json.keypairs', 'w') do |f|
  delegate_keys.each do |k|
    f.write "#{k[0]}   #{k[1]}\n"
  end
end
puts 'generated genesis.json.keypairs'

File.open('genesis.json.balancekeys', 'w') do |f|
  balance_keys.each do |k|
    f.write "#{k[2]} #{k[1]}\n"
  end
end
puts 'generated genesis.json.balancekeys'
