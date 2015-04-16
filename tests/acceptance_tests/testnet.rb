#!/usr/bin/env ruby

require 'fileutils'
require 'yaml'
require 'open3'
require 'io/console'
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

      ENV['BTS_BUILD'] = '../..' unless ENV['BTS_BUILD']
      @client_binary = ENV['BTS_BUILD'] + '/programs/client/bitsharestestnet_client'
      @client_binary = ENV['BTS_BUILD'] + '/programs/client/devshares_client' unless File.exist?(@client_binary)
      raise @client_binary + ' not found, please set BTS_BUILD env variable if you are using out of source builds' unless File.exist?(@client_binary)
    end

    def log(s)
      if @logger then @logger.info s else puts s end
    end

    def recreate_dir(dir); FileUtils.rm_rf(dir); Dir.mkdir(dir) end

    def td(path); "#{TEMPDIR}/#{path}"; end

    def for_each_delegate
      for i in 0..100
        yield "delegate#{i}"
      end
    end

    def full_bootstrap
      STDOUT.puts 'first time test net bootstrap.. this may take several minutes, please be patient..'
      log '========== full bootstrap ==========='
      FileUtils.rm_rf td('delegate_wallet_backup.json')
      FileUtils.rm_rf td('alice_wallet_backup.json')
      FileUtils.rm_rf td('bob_wallet_backup.json')
      @delegate_node.exec 'wallet_create', 'default', 'password'
      @delegate_node.exec 'wallet_unlock', '9999999', 'password'

      File.open('genesis.json.keypairs') do |f|
        counter = 0
        f.each_line do |l|
          pub_key, priv_key = l.split(' ')
          @delegate_node.exec 'wallet_import_private_key', priv_key, "delegate#{counter}"
          counter += 1
        end
      end

      sleep 1.0

      balancekeys = []
      File.open('genesis.json.balancekeys') do |f|
        f.each_line do |l|
          balancekeys << l.split(' ')[1]
        end
      end

      @delegate_node.exec 'wallet_import_private_key', balancekeys[0], "account0", true, true
      @delegate_node.exec 'wallet_import_private_key', balancekeys[1], "account1", true, true

      # for i in 0..100
      #   @delegate_node.exec 'wallet_delegate_set_block_production', "delegate#{i}", true
      # end
      @delegate_node.exec 'wallet_delegate_set_block_production', 'ALL', true

      @delegate_node.exec 'wallet_backup_create', td('delegate_wallet_backup.json')

      @alice_node.exec 'wallet_import_private_key', balancekeys[2], 'angel', true, true
      @alice_node.exec 'wallet_backup_create', td('alice_wallet_backup.json')

      @bob_node.exec 'wallet_import_private_key', balancekeys[3], 'angel', true, true
      @bob_node.exec 'wallet_backup_create', td('bob_wallet_backup.json')

    end

    def quick_bootstrap
      log '========== quick bootstrap ==========='
      @delegate_node.exec 'wallet_backup_restore', td('delegate_wallet_backup.json'), 'default', 'password'
      @alice_node.exec 'wallet_backup_restore', td('alice_wallet_backup.json'), 'default', 'password'
      @bob_node.exec 'wallet_backup_restore', td('bob_wallet_backup.json'), 'default', 'password'
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
      @delegate_node = BitSharesNode.new @client_binary, name: 'delegate', data_dir: td('delegate'), genesis: 'genesis.json', http_port: 5690, rpc_port: 6690, p2p_port: @p2p_port, delegate: true, logger: @logger
      @delegate_node.start

      @alice_node = BitSharesNode.new @client_binary, name: 'alice', data_dir: td('alice'), genesis: 'genesis.json', http_port: 5691, rpc_port: 6691, p2p_port: @p2p_port, logger: @logger
      @alice_node.start

      @bob_node = BitSharesNode.new @client_binary, name: 'bob', data_dir: td('bob'), genesis: 'genesis.json', http_port: 5692, rpc_port: 6692, p2p_port: @p2p_port, logger: @logger
      @bob_node.start

      nodes = [@delegate_node, @alice_node, @bob_node]
      wait_nodes(nodes)

      @delegate_node.exec 'wallet_open', 'default'
      @delegate_node.exec 'wallet_unlock', '9999999', 'password'
      @delegate_node.exec 'wallet_delegate_set_block_production', 'ALL', true

      @alice_node.exec 'wallet_open', 'default'
      @alice_node.exec 'wallet_unlock', '9999999', 'password'

      @bob_node.exec 'wallet_open', 'default'
      @bob_node.exec 'wallet_unlock', '9999999', 'password'

    end

    def create
      Dir.mkdir TEMPDIR unless Dir.exist? TEMPDIR
      recreate_dir td('delegate')
      recreate_dir td('alice')
      recreate_dir td('bob')

      quick = File.exist?(td('delegate_wallet_backup.json'))

      @delegate_node = BitSharesNode.new @client_binary, name: 'delegate', data_dir: td('delegate'), genesis: 'genesis.json', http_port: 5690, rpc_port: 6690, p2p_port: @p2p_port, delegate: true, logger: @logger
      @delegate_node.start

      @alice_node = BitSharesNode.new @client_binary, name: 'alice', data_dir: td('alice'), genesis: 'genesis.json', http_port: 5691, rpc_port: 6691, p2p_port: @p2p_port, logger: @logger
      @alice_node.start

      @bob_node = BitSharesNode.new @client_binary, name: 'bob', data_dir: td('bob'), genesis: 'genesis.json', http_port: 5692, rpc_port: 6692, p2p_port: @p2p_port, logger: @logger
      @bob_node.start

      wait_nodes([@delegate_node, @alice_node, @bob_node])

      if quick
        quick_bootstrap
      else
        @alice_node.exec 'wallet_create', 'default', 'password'
        @alice_node.exec 'wallet_unlock', '9999999', 'password'
        @bob_node.exec 'wallet_create', 'default', 'password'
        @bob_node.exec 'wallet_unlock', '9999999', 'password'
        full_bootstrap
      end

      @running = true
    end

    def shutdown
      log 'shutdown'
      @delegate_node.stop
      @alice_node.stop if @alice_node
      @bob_node.stop if @bob_node
      @running = false
    end

    def pause
      @alice_node.exec 'execute_command_line', 'enable_raw'
      @bob_node.exec 'execute_command_line', 'enable_raw'
      while true
        STDOUT.puts '@pause: use the following urls to access the nodes via browser:'
        STDOUT.puts "- delegate node: #{@delegate_node.url}"
        STDOUT.puts "- alice node: #{@alice_node.url}"
        STDOUT.puts "- bob node: #{@bob_node.url}"
        STDOUT.puts 'or press [d], [a], or [b] to have console access'
        STDOUT.puts 'or press any other key to shutdown testnet and continue..'
        c = ''
        begin
          STDIN.echo = false
          STDIN.raw!
          c = STDIN.getc
        ensure
          STDIN.echo = true
          STDIN.cooked!
        end
        if c == 'd'
          @delegate_node.interactive_mode
        elsif c == 'a'
          @alice_node.interactive_mode
        elsif c == 'b'
          @bob_node.interactive_mode
        else
          break
        end
      end
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
  testnet.pause
  puts 'exiting..'
  testnet.shutdown
end
