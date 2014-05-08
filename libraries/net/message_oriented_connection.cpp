#include <fc/thread/thread.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/thread/future.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/enum_type.hpp>

#include <bts/net/message_oriented_connection.hpp>
#include <bts/net/stcp_socket.hpp>

namespace bts { namespace net {
  namespace detail
  {
    class message_oriented_connection_impl
    {
    private:
      message_oriented_connection* _self;
      message_oriented_connection_delegate *_delegate;
      stcp_socket _sock;
      fc::future<void> _read_loop_done;
      uint64_t _bytes_received;
      uint64_t _bytes_sent;
      fc::time_point _last_message_received_time;
      fc::time_point _last_message_sent_time;
      fc::mutex _send_mutex;



      void read_loop();
      void start_read_loop();
    public:
      fc::tcp_socket& get_socket();
      void accept();
      void connect_to(const fc::ip::endpoint& remote_endpoint);
      void connect_to(const fc::ip::endpoint& remote_endpoint, const fc::ip::endpoint& local_endpoint);

      message_oriented_connection_impl(message_oriented_connection* self, message_oriented_connection_delegate* delegate = nullptr);
      void send_message(const message& message_to_send);
      void close_connection();
      uint64_t get_total_bytes_sent() const;
      uint64_t get_total_bytes_received() const;
      fc::time_point get_last_message_sent_time() const;
      fc::time_point get_last_message_received_time() const;
    };

    message_oriented_connection_impl::message_oriented_connection_impl(message_oriented_connection* self, message_oriented_connection_delegate* delegate) : 
      _self(self),
      _delegate(delegate),
      _bytes_received(0),
      _bytes_sent(0)
    {
    }

    fc::tcp_socket& message_oriented_connection_impl::get_socket()
    {
      return _sock.get_socket();
    }

    void message_oriented_connection_impl::accept()
    {
      _sock.accept();
      _read_loop_done = fc::async([=](){ read_loop(); });
    }

    void message_oriented_connection_impl::connect_to(const fc::ip::endpoint& remote_endpoint)
    {
      _sock.connect_to(remote_endpoint);
      _read_loop_done = fc::async([=](){ read_loop(); });
    }

    void message_oriented_connection_impl::connect_to(const fc::ip::endpoint& remote_endpoint, const fc::ip::endpoint& local_endpoint)
    {
      _sock.connect_to(remote_endpoint, local_endpoint);
      _read_loop_done = fc::async([=](){ read_loop(); });
    }


    void message_oriented_connection_impl::read_loop()
    {
      const int BUFFER_SIZE = 16;
      const int LEFTOVER = BUFFER_SIZE - sizeof(message_header);
      assert(BUFFER_SIZE >= sizeof(message_header));

      try 
      {
        message m;
        while( true )
        {
          char buffer[BUFFER_SIZE];
          _sock.read(buffer, BUFFER_SIZE);
          _bytes_received += BUFFER_SIZE;
          memcpy((char*)&m, buffer, sizeof(message_header));

          size_t remaining_bytes_with_padding = 16 * ((m.size - LEFTOVER + 15) / 16);
          m.data.resize(LEFTOVER + remaining_bytes_with_padding); //give extra 16 bytes to allow for padding added in send call
          std::copy(buffer + sizeof(message_header), buffer + sizeof(buffer), m.data.begin());
          if (remaining_bytes_with_padding)
          {
            _sock.read(&m.data[LEFTOVER], remaining_bytes_with_padding);
            _bytes_received += remaining_bytes_with_padding;
          }
          m.data.resize(m.size); // truncate off the padding bytes

          _last_message_received_time = fc::time_point::now();

          try 
          {
            // message handling errors are warnings...
            _delegate->on_message(_self, m);
          } 
          /// Dedicated catches needed to distinguish from general fc::exception
          catch ( fc::canceled_exception& e ) { throw e; }
          catch ( fc::eof_exception& e ) { throw e; }
          catch ( fc::exception& e ) 
          { 
            /// Here loop should be continued so exception should be just caught locally.
            wlog( "message transmission failed ${er}", ("er", e.to_detail_string() ) );
          }
        }
      } 
      catch ( const fc::canceled_exception& e )
      {
        wlog( "disconnected ${e}", ("e", e.to_detail_string() ) );
        _delegate->on_connection_closed(_self);
      }
      catch ( const fc::eof_exception& e )
      {
        wlog( "disconnected ${e}", ("e", e.to_detail_string() ) );
        _delegate->on_connection_closed(_self);
      }
      catch ( fc::exception& e )
      {
        elog( "disconnected ${er}", ("er", e.to_detail_string() ) );
        _delegate->on_connection_closed(_self);

        FC_RETHROW_EXCEPTION( e, warn, "disconnected ${e}", ("e", e.to_detail_string() ) );
      }
      catch ( ... )
      {
        _delegate->on_connection_closed(_self);
        FC_THROW_EXCEPTION( unhandled_exception, "disconnected: {e}", ("e", fc::except_str() ) );
      }
    }

    void message_oriented_connection_impl::send_message(const message& message_to_send)
    {
      try 
      {
        size_t size_of_message_and_header = sizeof(message_header) + message_to_send.size;
        //pad the message we send to a multiple of 16 bytes
        size_t size_with_padding = 16 * ((size_of_message_and_header + 15) / 16);
        std::unique_ptr<char[]> padded_message(new char[size_with_padding]);
        memcpy(padded_message.get(), (char*)&message_to_send, sizeof(message_header));
        memcpy(padded_message.get() + sizeof(message_header), message_to_send.data.data(), message_to_send.size );
        fc::scoped_lock<fc::mutex> lock(_send_mutex);
        _sock.write(padded_message.get(), size_with_padding);
        _sock.flush();
        _bytes_sent += size_with_padding;
        _last_message_sent_time = fc::time_point::now();
      } FC_RETHROW_EXCEPTIONS( warn, "unable to send message" );    
    }

    void message_oriented_connection_impl::close_connection()
    {
      _sock.close();
    }

    uint64_t message_oriented_connection_impl::get_total_bytes_sent() const
    {
      return _bytes_sent;
    }

    uint64_t message_oriented_connection_impl::get_total_bytes_received() const
    {
      return _bytes_received;
    }

    fc::time_point message_oriented_connection_impl::get_last_message_sent_time() const
    {
      return _last_message_sent_time;
    }

    fc::time_point message_oriented_connection_impl::get_last_message_received_time() const
    {
      return _last_message_received_time;
    }

  } // end namespace bts::net::detail


  message_oriented_connection::message_oriented_connection(message_oriented_connection_delegate* delegate) : 
    my(new detail::message_oriented_connection_impl(this, delegate))
  {
  }

  message_oriented_connection::~message_oriented_connection()
  {
  }

  fc::tcp_socket& message_oriented_connection::get_socket()
  {
    return my->get_socket();
  }

  void message_oriented_connection::accept()
  {
    my->accept();
  }

  void message_oriented_connection::connect_to(const fc::ip::endpoint& remote_endpoint)
  {
    my->connect_to(remote_endpoint);
  }

  void message_oriented_connection::connect_to(const fc::ip::endpoint& remote_endpoint, const fc::ip::endpoint& local_endpoint)
  {
    my->connect_to(remote_endpoint, local_endpoint);
  }
  
  void message_oriented_connection::send_message(const message& message_to_send)
  {
    my->send_message(message_to_send);
  }

  void message_oriented_connection::close_connection()
  {
    my->close_connection();
  }

  uint64_t message_oriented_connection::get_total_bytes_sent() const
  {
    return my->get_total_bytes_sent();
  }

  uint64_t message_oriented_connection::get_total_bytes_received() const
  {
    return my->get_total_bytes_received();
  }

  fc::time_point message_oriented_connection::get_last_message_sent_time() const
  {
    return my->get_last_message_sent_time();
  }

  fc::time_point message_oriented_connection::get_last_message_received_time() const
  {
    return my->get_last_message_received_time();
  }

} } // end namespace bts::net
