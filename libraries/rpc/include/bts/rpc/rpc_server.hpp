#pragma once
#include <memory>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/log_message.hpp>
#include <bts/client/client.hpp>
#include <bts/api/api_metadata.hpp>

namespace bts { namespace client {
  class client;
} }

namespace bts { namespace rpc {
  namespace detail { class rpc_server_impl; }

  /**
  *  @class rpc_server
  *  @brief provides a json-rpc interface to the bts client
  */
  class rpc_server
  {
  public:
    struct config
    {
      config():rpc_user("user"),
               rpc_password("password"),
               rpc_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0")),
               httpd_endpoint(fc::ip::endpoint::from_string("127.0.0.1:0")),
               htdocs("./htdocs"){}
      std::string      rpc_user;
      std::string      rpc_password;
      fc::ip::endpoint rpc_endpoint;
      fc::ip::endpoint httpd_endpoint;
      fc::path         htdocs;

      bool is_valid() const; /* Currently just checks if rpc port is set */
    };

    rpc_server(bts::client::client* client);
    virtual ~rpc_server();

    bool        configure( const config& cfg );

    /// used to invoke json methods from the cli without going over the network
    fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments);

    const bts::api::method_data& get_method_data(const std::string& method_name);
    std::vector<bts::api::method_data> get_all_method_data() const;

    void close(); // shut down the RPC server
    void wait(); // wait until the RPC server is shut down (via the above close(), or by processing a "stop" RPC call)
    void wait_on_quit();
    void shutdown_rpc_server();
    std::string help(const std::string& command_name) const;
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

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::rpc::rpc_server::config, (rpc_user)(rpc_password)(rpc_endpoint)(httpd_endpoint)(htdocs) )
