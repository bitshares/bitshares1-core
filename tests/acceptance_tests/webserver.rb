require 'thread'
require 'socket'

class WebServer

  attr_reader :requests

  READBYTES = 256

  def initialize
    @tcp_server = TCPServer.new('localhost', 23232)
    @requests = []
  end

  def read_until_content_length(socket)
    buffer = ''
    content_length = 0
    while (tmp = socket.recv(READBYTES))
      buffer << tmp
      if content_length == 0 and buffer =~ /Content\-Length: (\d+)/mi
        content_length = $1.to_i
        break
      end
    end
    return buffer, content_length
  end

  def read_content(buffer)
    content = ''
    new_lines_sequence = 0
    in_content = false
    buffer.each_byte do |i|
      if in_content
        content << i.chr
      else
        if i.chr == "\n" or i.chr == "\r"
          new_lines_sequence += 1 if i.chr == "\n"
          in_content = new_lines_sequence >= 2
        else
          new_lines_sequence = 0
        end
      end
    end
    return content
  end

  def read_data(socket)
    buffer, content_length = read_until_content_length(socket)
    content = read_content(buffer)
    while content.length < content_length
      if content.length == 0
        buffer << socket.recv(READBYTES)
        content = read_content(buffer)
      else
        to_read = content_length - content.length
        to_read = READBYTES if to_read > READBYTES
        tmp = socket.recv(to_read)
        content << tmp
      end
    end
    return content
  end

  def start
    @requests.clear
    @thread = Thread.new do
      begin
        loop do
          socket = @tcp_server.accept
          request = read_data(socket)
          #STDOUT.puts "--> request --> <#{request}>"
          @requests << request
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
