require 'thread'
require 'socket'

class WebServer

  attr_reader :requests

  def initialize
    @tcp_server = TCPServer.new('localhost', 23232)
    @requests = []
  end

  def read_data(socket)
    buffer = ''
    while (tmp = socket.recv(1024))
      buffer << tmp
      break
    end
    return buffer
  end

  def add_request(request)
    header = true
    data = ''
    request.each_line do |line|
      s = line.chomp
      header = false if s.empty?
      data << s if !header and !s.empty?
    end
    @requests << data
  end

  def start
    @requests.clear
    @thread = Thread.new do
      begin
        loop do
          socket = @tcp_server.accept
          request = read_data(socket)
          add_request(request)
          response = "ok\n"
          socket.print "HTTP/1.1 200 OK\r\n" +
                           "Content-Type: text/plain\r\n" +
                           "Content-Length: #{response.bytesize}\r\n" +
                           "Connection: close\r\n"
          socket.print "\r\n"
          socket.print response
          socket.close
        end
      rescue Exception => e
        puts "Error: #{e.message}"
        puts e.backtrace.join("\n")
        @thread.exit
      end
    end
  end

  def shutdown
    @thread.exit
  end

end

if $0 == __FILE__

  webserver = WebServer.new
  webserver.start
  puts 'webserver is running, press any key to exit..'
  STDIN.getc
  webserver.shutdown
  puts webserver.requests
  puts 'finished'
end
