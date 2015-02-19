#include <bts/rpc/rpc_client.hpp>
#include <bts/net/stcp_socket.hpp>
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/utilities/padding_ostream.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/digest.hpp>
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
      void connect_to(const fc::ip::endpoint& remote_endpoint, const blockchain::public_key_type& remote_public_key);
      bool login(const std::string& username, const std::string& password);
    };

    void rpc_client_impl::connect_to(const fc::ip::endpoint& remote_endpoint,
                                     const bts::blockchain::public_key_type& remote_public_key)
    {
       fc::buffered_istream_ptr buffered_istream;
       fc::buffered_ostream_ptr buffered_ostream;

       if( remote_public_key != bts::blockchain::public_key_type() )
       {
          net::stcp_socket_ptr socket = std::make_shared<bts::net::stcp_socket>();

          try
          {
             socket->connect_to(remote_endpoint);
          }
          catch ( const fc::exception& e )
          {
             elog( "fatal: error opening RPC socket to endpoint ${endpoint}: ${e}", ("endpoint", remote_endpoint)("e", e.to_detail_string() ) );
             throw;
          }

          std::vector<char> packed_signature(80, ' ');
          FC_ASSERT( socket->readsome(packed_signature.data(), packed_signature.size()) == 80, "Unexpected number of bytes read from RPC socket" );
          fc::ecc::compact_signature signature;
          signature = fc::raw::unpack<fc::ecc::compact_signature>(packed_signature);
          wdump((signature));
          FC_ASSERT(blockchain::public_key_type(fc::ecc::public_key(signature, fc::digest(socket->get_shared_secret())))
                    == remote_public_key,
                    "Unable to establish secure connection with server.");

          buffered_istream = std::make_shared<fc::buffered_istream>(socket);
          buffered_ostream = std::make_shared<utilities::padding_ostream<>>(socket);
       } else {
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

          buffered_istream = std::make_shared<fc::buffered_istream>(socket);
          buffered_ostream = std::make_shared<fc::buffered_ostream>(socket);
       }

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
     try
     {
        my->_json_exec_loop_complete.cancel_and_wait("~rpc_client()");
     } catch (const fc::exception& e) {
        wlog("Caught exception ${e} while canceling json_connection exec loop", ("e", e));
     }
  }

  void rpc_client::connect_to(const fc::ip::endpoint& remote_endpoint,
                              const bts::blockchain::public_key_type& remote_public_key)
  {
    my->connect_to(remote_endpoint, remote_public_key);
  }

  bool rpc_client::login(const std::string& username, const std::string& password)
  {
    return my->login(username, password);
  }
  
  fc::rpc::json_connection_ptr rpc_client::get_json_connection() const
  {
     return my->_json_connection;
  }

  void rpc_client::reset_json_connection()
  {
     my->_json_connection->close();
     auto ptr_copy = my->_json_connection;
     fc::schedule([ptr_copy] { /* Do nothing; just let ptr_copy go out of scope */ },
                  fc::time_point::now() + fc::seconds(1), __FUNCTION__);
     my->_json_connection.reset();
  }

} } // bts::rpc
