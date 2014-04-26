#pragma once
#include <memory>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/log_message.hpp>
#include <bts/client/client.hpp>

namespace bts { namespace rpc {
  using namespace bts::client;

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
               rpc_endpoint(fc::ip::endpoint::from_string("127.0.0.1:9988")),
               httpd_endpoint(fc::ip::endpoint::from_string("127.0.0.1:9989")),
               htdocs("./htdocs"){}
      std::string      rpc_user;
      std::string      rpc_password;
      fc::ip::endpoint rpc_endpoint;
      fc::ip::endpoint httpd_endpoint;
      fc::path         htdocs;

      bool is_valid() const;
    };

    enum method_prerequisites
    {
      no_prerequisites     = 0,
      json_authenticated   = 1,
      wallet_open          = 2,
      wallet_unlocked      = 4,
      connected_to_network = 8,
    };
    struct parameter_data
    {
      std::string name;
      std::string type;
      bool        required;
    };
    typedef std::function<fc::variant(const fc::variants& params)> json_api_method_type;
    struct method_data
    {
      std::string                 name;
      json_api_method_type        method;
      std::string                 description;
      std::string                 return_type;
      std::vector<parameter_data> parameters;
      uint32_t                    prerequisites;
      std::string                 detailed_description;
    };

    rpc_server();
    ~rpc_server();

    client_ptr  get_client()const;
    void        set_client( const client_ptr& c );
    void        configure( const config& cfg );

    /// used to invoke json methods from the cli without going over the network
    fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments);

    const method_data& get_method_data(const std::string& method_name);

    /** can be called for methods that require the user to be logged in via
    *  RPC.
    */
    void check_connected_to_network();
    void check_wallet_unlocked();
    void check_wallet_is_open();
  protected:
    friend class bts::rpc::detail::rpc_server_impl;

    void register_method(method_data method);
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
