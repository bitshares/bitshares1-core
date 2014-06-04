#include <bts/rpc/rpc_client.hpp>
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/thread/future.hpp>

#include <bts/rpc_stubs/common_api_rpc_client.hpp>

namespace bts { namespace rpc { 

  namespace detail 
  {
    class rpc_client_impl : public bts::rpc_stubs::common_api_rpc_client
    {
    public:
      fc::rpc::json_connection_ptr _json_connection;
      fc::future<void> _json_exec_loop_complete;

      virtual fc::rpc::json_connection_ptr get_json_connection() const override { return _json_connection; }
      void connect_to(const fc::ip::endpoint& remote_endpoint);
      bool login(const std::string& username, const std::string& password);
    };

    void rpc_client_impl::connect_to(const fc::ip::endpoint& remote_endpoint)
    {
      fc::tcp_socket_ptr socket = std::make_shared<fc::tcp_socket>();
      try 
      {
        socket->connect_to(remote_endpoint);
      }
      catch ( const fc::exception& e )
      {
        elog( "fatal: error opening RPC socket to endpoint ${endpoint}: ${e}", ("endpoint", remote_endpoint)("e", e.to_detail_string() ) );
        throw;
      }

      fc::buffered_istream_ptr buffered_istream = std::make_shared<fc::buffered_istream>(socket);
      fc::buffered_ostream_ptr buffered_ostream = std::make_shared<fc::buffered_ostream>(socket);

      _json_connection = std::make_shared<fc::rpc::json_connection>(std::move(buffered_istream), 
                                                                    std::move(buffered_ostream));
      _json_exec_loop_complete = fc::async([=](){ _json_connection->exec(); }, "json exec loop");
    }
    bool rpc_client_impl::login(const std::string& username, const std::string& password)
    {
      return _json_connection->call<bool>("login", username, password);
    }

  } // end namespace detail


  rpc_client::rpc_client() :
    my(new detail::rpc_client_impl)
  {
  }

  rpc_client::~rpc_client()
  {
  }

  void rpc_client::connect_to(const fc::ip::endpoint& remote_endpoint)
  {
    my->connect_to(remote_endpoint);
  }

  bool rpc_client::login(const std::string& username, const std::string& password)
  {
    return my->login(username, password);
  }
  
  fc::rpc::json_connection_ptr rpc_client::get_json_connection() const
  {
    return my->_json_connection;
  }

} } // bts::rpc
