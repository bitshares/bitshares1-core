require 'rubygems'
require 'bundler'

#log = File.new("log/sinatra.log", "a+")
#STDOUT.reopen(log)
#STDERR.reopen(log)

Bundler.require


require 'rack/reverse_proxy'

use Rack::ReverseProxy do
  reverse_proxy '/rpc', 'http://localhost:9989/rpc'
end

require './app'
run App.new
