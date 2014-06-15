#pragma once
#include <memory>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/log_message.hpp>
#include <bts/api/api_metadata.hpp>
#include <bts/rpc_stubs/common_api_rpc_server.hpp>
#include <bts/client/client.hpp>
#include <fc/network/http/server.hpp>

namespace bts { namespace client {
  class client;
} }

namespace bts { namespace rpc {
  namespace detail { class rpc_server_impl; }

  typedef std::map<std::string, bts::api::method_data> method_map_type;
  typedef std::function<void( const fc::path& filename, const fc::http::server::response&)> http_callback_type; 
  /**
  *  @class rpc_server
  *  @brief provides a json-rpc interface to the bts client
  */
  class rpc_server
  {
     public:
       rpc_server(bts::client::client* client);
       virtual ~rpc_server();

       bool        configure( const bts::client::rpc_server_config& cfg );

       /// used to invoke json methods from the cli without going over the network
       fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments);

       const bts::api::method_data& get_method_data(const std::string& method_name);
       std::vector<bts::api::method_data> get_all_method_data() const;

       void close(); // shut down the RPC server
       // wait until the RPC server is shut down (via the above close(), or by processing a "stop" RPC call)
       void wait_till_rpc_server_shutdown();
       void shutdown_rpc_server();
       std::string help(const std::string& command_name) const;

       method_map_type meta_help()const;

       void set_http_file_callback(  const http_callback_type& );

       fc::optional<fc::ip::endpoint> get_rpc_endpoint() const;
       fc::optional<fc::ip::endpoint> get_httpd_endpoint() const;
     protected:
       friend class bts::rpc::detail::rpc_server_impl;

       void validate_method_data(bts::api::method_data method);
       void register_method(bts::api::method_data method);

     private:
         std::unique_ptr<detail::rpc_server_impl> my;
  };

  typedef std::shared_ptr<rpc_server> rpc_server_ptr;


  class exception : public fc::exception {
  public:
    exception( fc::log_message&& m );
    exception( fc::log_messages );
    exception( const exception& c );
    exception();
    virtual const char* what() const throw() override = 0;
    virtual int32_t get_rpc_error_code() const = 0;
  };

#define RPC_DECLARE_EXCEPTION(TYPE) \
  class TYPE : public exception \
  { \
  public: \
    TYPE( fc::log_message&& m ); \
    TYPE( fc::log_messages ); \
    TYPE( const TYPE& c ); \
    TYPE(); \
    virtual const char* what() const throw() override; \
    int32_t get_rpc_error_code() const override; \
  };

RPC_DECLARE_EXCEPTION(rpc_misc_error_exception)

RPC_DECLARE_EXCEPTION(rpc_client_not_connected_exception)

RPC_DECLARE_EXCEPTION(rpc_wallet_unlock_needed_exception)
RPC_DECLARE_EXCEPTION(rpc_wallet_passphrase_incorrect_exception)
RPC_DECLARE_EXCEPTION(rpc_wallet_open_needed_exception)

} } // bts::rpc

