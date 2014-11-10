#!/usr/bin/env ruby

require 'fileutils'
require 'yaml'
require 'open3'
require_relative './client.rb'

module BitShares

  class TestNet

    attr_reader :delegate_node, :alice_node, :bob_node, :running

    TEMPDIR = 'tmp'

    def initialize(logger = nil)
      @logger = logger
      @delegate_node = nil
      @alice_node = nil
      @bob_node = nil
      @p2p_port = 10000 + Random.rand(10000)
      @running = false

      raise 'BTS_BUILD env variable is not set' unless ENV['BTS_BUILD']
      @client_binary = ENV['BTS_BUILD'] + '/programs/client/bitshares_client'
    end

    def log(s)
      if @logger then @logger.info s else puts s end
    end

    def recreate_dir(dir); FileUtils.rm_rf(dir); Dir.mkdir(dir) end

    def td(path); "#{TEMPDIR}/#{path}"; end

    def create_client_node(dir, port, create_wallet=true)
      clientnode = BitSharesNode.new @client_binary, name: dir, data_dir: td(dir), genesis: 'genesis.json', http_port: port, p2p_port: @p2p_port, logger: @logger
      clientnode.start(false)
      if create_wallet
        clientnode.exec 'enable_raw'
        clientnode.exec 'wallet_create', 'default', '123456789'
        clientnode.exec 'wallet_unlock', '9999999', '123456789'
      end
      return clientnode
    end

    def for_each_delegate
      for i in 0..100
        yield "delegate#{i}"
      end
    end

    def full_bootstrap
      log '========== full bootstrap ==========='
      FileUtils.rm_rf td('delegate_wallet_backup.json')
      FileUtils.rm_rf td('alice_wallet_backup.json')
      FileUtils.rm_rf td('bob_wallet_backup.json')
      @delegate_node.exec 'wallet_create', 'default', '123456789'
      @delegate_node.exec 'wallet_unlock', '9999999', '123456789'

      File.open('genesis.json.keypairs') do |f|
        counter = 0
        f.each_line do |l|
          pub_key, priv_key = l.split(' ')
          @delegate_node.exec 'wallet_import_private_key', priv_key, "delegate#{counter}"
          counter += 1
          #break if counter > 10
        end
      end

      sleep 1.0

      for i in 0..10
        @delegate_node.exec 'wallet_delegate_set_block_production', "delegate#{i}", true
      end

      balancekeys = []
      File.open('genesis.json.balancekeys') do |f|
        f.each_line do |l|
          balancekeys << l.split(' ')[1]
        end
      end

      @delegate_node.exec 'wallet_import_private_key', balancekeys[0], "account0", true, true
      @delegate_node.exec 'wallet_import_private_key', balancekeys[1], "account1", true, true

      for i in 0..100
        @delegate_node.exec 'wallet_delegate_set_block_production', "delegate#{i}", true
      end

      @delegate_node.exec 'wallet_backup_create', td('delegate_wallet_backup.json')

      @alice_node.exec 'wallet_import_private_key', balancekeys[2], 'angel', true, true
      @alice_node.exec 'wallet_backup_create', td('alice_wallet_backup.json')

      @bob_node.exec 'wallet_import_private_key', balancekeys[3], 'angel', true, true
      @bob_node.exec 'wallet_backup_create', td('bob_wallet_backup.json')
    end

    def quick_bootstrap
      log '========== quick bootstrap ==========='
      @delegate_node.exec 'wallet_backup_restore', td('delegate_wallet_backup.json'), 'default', '123456789'
      @alice_node.exec 'wallet_backup_restore', td('alice_wallet_backup.json'), 'default', '123456789'
      @bob_node.exec 'wallet_backup_restore', td('bob_wallet_backup.json'), 'default', '123456789'
    end

    def wait_nodes(nodes)
      while nodes.length > 0
        nodes.each_with_index do |n, i|
          #log "waiting for node #{n.name}"
          line = n.stdout_gets
          #log "#{n.name}: #{line}"
          if line.nil?
            puts "Error: node #{n.name} is down"
            log "Error: node #{n.name} is down"
            nodes.delete_at(i)
          end
          if line and line.include? 'Starting HTTP JSON RPC server on port'
            log "node #{n.name} is up"
            nodes.delete_at(i)
          end
        end
      end
    end

    def start
      @delegate_node = BitSharesNode.new @client_binary, name: 'delegate', data_dir: td('delegate'), genesis: 'genesis.json', http_port: 5690, p2p_port: @p2p_port, delegate: true, logger: @logger
      @delegate_node.start(false)

      @alice_node = BitSharesNode.new @client_binary, name: 'alice', data_dir: td('alice'), genesis: 'genesis.json', http_port: 5691, p2p_port: @p2p_port, logger: @logger
      @alice_node.start(false)

      @bob_node = BitSharesNode.new @client_binary, name: 'bob', data_dir: td('bob'), genesis: 'genesis.json', http_port: 5692, p2p_port: @p2p_port, logger: @logger
      @bob_node.start(false)

      nodes = [@delegate_node, @alice_node, @bob_node]
      wait_nodes(nodes)
      nodes.each{ |n| n.exec 'enable_raw' }
    end

    def create
      Dir.mkdir TEMPDIR unless Dir.exist? TEMPDIR
      recreate_dir td('delegate')
      recreate_dir td('alice')
      recreate_dir td('bob')

      quick = File.exist?(td('delegate_wallet_backup.json'))

      @delegate_node = BitSharesNode.new @client_binary, name: 'delegate', data_dir: td('delegate'), genesis: 'genesis.json', http_port: 5690, p2p_port: @p2p_port, delegate: true, logger: @logger
      @delegate_node.start(false)

      @alice_node = create_client_node('alice', 5691, !quick)
      @bob_node = create_client_node('bob', 5692, !quick)

      wait_nodes([@delegate_node, @alice_node, @bob_node])

      if quick
        quick_bootstrap
      else
        full_bootstrap
      end

      @running = true
    end

    def shutdown
      log 'shutdown'
      @delegate_node.exec 'quit'
      @alice_node.exec 'quit' if @alice_node
      @bob_node.exec 'quit' if @bob_node
      @running = false
    end

  end

end

if $0 == __FILE__
  testnet = BitShares::TestNet.new
  if ARGV[0] == '--create'
    testnet.create
  else
    testnet.start
  end
  puts 'testnet is running, press any key to exit..'
  STDIN.getc
  testnet.shutdown
  puts 'finished'
end