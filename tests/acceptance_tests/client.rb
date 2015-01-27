#!/usr/bin/env ruby

require 'open3'
require 'json'
require 'readline'
require_relative './bitshares_api.rb'

class BitSharesNode

  attr_reader :rpc_instance, :command, :name, :url, :options

  class Error < RuntimeError; end

  def initialize(client_binary, options)
    @client_binary, @options = client_binary, options
    @name = options[:name]
    @logger = options[:logger]
    @handler = nil

    @command = @client_binary.clone
    @command << " --data-dir #{options[:data_dir]}"
    @command << " --genesis-config #{options[:genesis]}"
    @command << " --min-delegate-connection-count=0"
    @command << " --statistics-enabled"
    @command << " --server"
    @command << " --rpcuser=user"
    @command << " --rpcpassword=pass"
    @command << " --httpport=#{options[:http_port]}"
    @command << " --rpcport=#{options[:rpc_port]}"
    @command << " --upnp=false"
    if options[:delegate]
      @command << " --p2p-port=#{options[:p2p_port]}"
    else
      @command << " --connect-to=127.0.0.1:#{options[:p2p_port]}"
    end
    @command << " --disable-default-peers"
    
    @url = "http://localhost:#{options[:http_port]}"
  end

  def log(s)
    if @logger then @logger.info s else puts s end
  end
  
  def start(wait=false)
    log "starting node '#{@name}', http port: #{@options[:http_port]}, rpc port: #{@options[:rpc_port]}, p2p port: #{@options[:p2p_port]} \n#{@command}"
    log ""

    stdin, out, wait_thr = Open3.popen2e(@command)
    @handler = {stdin: stdin, out: out, wait_thr: wait_thr}
    while wait
      if select([out], nil, nil, 0.2) and out.eof?
        log "process exited and doesn't have output queued."
        break
      else
        line = out.gets
        log line
        break if line and line.include? "Starting HTTP JSON RPC server on port #{@options[:http_port]}"
      end
    end

    sleep 1.0
    
    @rpc_instance = BitShares::API::Rpc.new(@options[:http_port], 'user', 'pass', ignore_errors: false, logger: @logger, instance_name: @name)

    return

  end

  def stop
    @handler[:stdin].write("quit\n")
    begin
      Process.wait(@handler[:wait_thr].pid)
      @rpc_instance = nil
    rescue
    end
  end
  
  def running
    @rpc_instance != nil
  end

  def stdout_gets
    line = nil
    out = @handler[:out]
    unless select([out], nil, nil, 0.2) and out.eof?
      line = out.gets
    end
    return line
  end

  def exec(method, *params)
    raise Error, "rpc instance is not defined, make sure the node is started" unless @rpc_instance
    begin
      @rpc_instance.request(method, params)
    rescue EOFError => e
      STDOUT.puts "encountered EOFError, #{@name} instance may have crashed"
      pid = @handler[:wait_thr].pid
      STDOUT.puts "waiting for process #{pid} to exit"
      begin
        Process.wait(pid)
      rescue Errno::ECHILD
        raise Error, "#{name} (pid:#{pid}) instance was crashed or exited unexpectedly"
      end
      # while s = stdout_gets
      #   STDOUT.puts s
      # end
      raise e
    end
  end

  def wait_new_block
    inititial_block_num = new_block_num = exec('info')['blockchain_head_block_num']
    while inititial_block_num == new_block_num
      sleep 1.0
      new_block_num = exec('info')['blockchain_head_block_num']
    end
  end

  def get_config
    JSON.parse( IO.read("#{@options[:data_dir]}/config.json") )
  end

  def save_config(config)
    File.write("#{@options[:data_dir]}/config.json", JSON.pretty_generate(config))
  end

  @@completion_list = nil
  @@completion_proc = proc { |s| @@completion_list.grep( /^#{Regexp.escape(s)}/ ) }

  def interactive_mode
    STDOUT.puts "\nentering node '#{@name}' interactive mode, enter 'exit' or 'quit' to exit"
    ignore_errors = @rpc_instance.ignore_errors
    @rpc_instance.ignore_errors = true
    echo_off = @rpc_instance.echo_off
    @rpc_instance.echo_off = true

    unless @@completion_list
      @@completion_list = []
      res = @rpc_instance.request('help')
      res.split("\n").each do |l|
        c = l.chomp.split(' ')[0]
        @@completion_list << c
      end
      Readline.completion_append_character = ' '
      Readline.completion_proc = @@completion_proc
    end

    while command = Readline.readline('→ ', true)
      break if command.nil?
      command.chomp!
      break if command == 'exit' or command == 'quit'
      next if command.empty?
      res = @rpc_instance.request('execute_command_line', [command])
      STDOUT.puts res.gsub('\n', "\n") if res and not res.empty?
    end
    @rpc_instance.ignore_errors = ignore_errors
    @rpc_instance.echo_off = echo_off
  end

end

if $0 == __FILE__
  client_binary = "#{ENV['BTS_BUILD']}/programs/client/bitshares_client"
  client_node = BitSharesNode.new client_binary, data_dir: "tmp/client_a", genesis: "test_genesis.json", http_port: 5680, rpc_port: 6680, delegate: false
  client_node.start
  client_node.exec 'create', 'default', 'password'
  client_node.exec 'unlock', '9999999', 'password'
  puts "client node is up and running on port 5680"
  STDIN.getc
end
