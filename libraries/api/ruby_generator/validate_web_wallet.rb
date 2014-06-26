#!/usr/bin/env ruby

require 'json'

class WebWalletValidator

  def initialize(web_wallet_path)
    @methods = {}
    @web_wallet_path = web_wallet_path
    load_api_methods JSON.parse(IO.read('../wallet_api.json'))
    load_api_methods JSON.parse(IO.read('../network_api.json'))
    load_api_methods JSON.parse(IO.read('../blockchain_api.json'))
    @methods['batch'] = [2, 0]
    @methods['execute_command_line'] = [1,0]
  end

  def load_api_methods(json)
    json['methods'].each do |m|
      params = m['parameters'].length
      optional = m['parameters'].inject(0) { |sum, p| p.key?('default_value') ? sum + 1 : sum }
      @methods[m['method_name']] = [params,optional]
      #puts "#{m['method_name']} #{[params, optional]}"
    end
  end

  def run
    check_dir '/app/js/controllers/*.coffee'
    check_dir '/app/js/services/*.coffee'
  end

  def check_dir(dirpath)
    Dir[@web_wallet_path + dirpath].each do |filepath|
      filename = filepath[/([\w]+\/[\w.]+)$/]
      #puts "---- #{filename} ----"
      check_file(filepath, filename)
    end
  end

  def check_file(filepath, filename)
    File.open(filepath,'r') do |f|
      f.each_with_index do |l, n|
        if l =~ /WalletAPI\.(\w+)\s?\(\s?([\s\w\.,\$]+\s?)\)\.then/ ||
           l =~ /@wallet_api\.(\w+)\s?\(\s?([\s\w\.,\$]+\s?)\)\.then/
          check_method('wallet_' + $1, $2, filename, n+1)
        end

        if l =~ /BlockchainAPI\.(\w+)\s?\(\s?([\s\w\.,\$]+\s?)\)\.then/ ||
           l =~ /@blockchain_api\.(\w+)\s?\(\s?([\s\w\.,\$]+\s?)\)\.then/
          check_method('blockchain_' + $1, $2, filename, n+1)
        end

        if l =~ /NetworkAPI\.(\w+)\s?\(\s?([\s\w\.,\$]+\s?)\)\.then/ ||
           l =~ /@network_api\.(\w+)\s?\(\s?([\s\w\.,\$]+\s?)\)\.then/
          check_method('network_' + $1, $2, filename, n+1)
        end

        if l =~ /\.request\s?\(\s?(['"\w]+)\s?,\s+\[(['"\s\w\.,\$]+)\]\)\.then/
          args = $2
          check_method($1.gsub(/['"]/, ''), args, filename, n+1)
        end

      end
    end
  end

  def check_method(method, args, filename, linenum)
    unless @methods.key? method
      puts "#{filename}:#{linenum} : api method '#{method}' not found"
      return
    end
    num_args = args ? args.split(',').length : 0
    method_num_args = @methods[method][0]
    method_opt_args = @methods[method][1]
    if num_args > method_num_args or num_args < (method_num_args - method_opt_args)
      puts "#{filename}:#{linenum} : method '#{method}' has incorrect number of args, #{num_args} vs. #{method_num_args} (#{method_opt_args} optional) "
    end
  end



end


path_to_web_wallet = '../../../programs/web_wallet'

validator = WebWalletValidator.new path_to_web_wallet

validator.run

