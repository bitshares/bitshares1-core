#pragma once
#include <memory>
#include <fc/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/log_message.hpp>
#include <bts/client/client.hpp>

namespace fc { namespace rpc { 
   class json_connection; 
   typedef std::shared_ptr<json_connection> json_connection_ptr;
} }

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
              std::string      rpc_user;
              std::string      rpc_password;
              fc::ip::endpoint rpc_endpoint;

              bool is_valid() const;
           };
           rpc_server();
           ~rpc_server();

           client_ptr get_client()const;
           void       set_client( const client_ptr& c );
           void       configure( const config& cfg );
           fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments);
        protected:
           friend class bts::rpc::detail::rpc_server_impl;

           virtual void register_methods( const fc::rpc::json_connection_ptr& con );

           /** can be called for methods that require the user to be logged in via
            *  RPC.
            */
           void check_json_connection_authenticated( fc::rpc::json_connection* con );

        private:
           std::unique_ptr<detail::rpc_server_impl> my;
   };
   typedef std::shared_ptr<rpc_server> rpc_server_ptr;


#define RPC_DECLARE_EXCEPTION(TYPE) \
  class TYPE : public fc::exception \
  { \
  public: \
    TYPE( fc::log_message&& m ); \
    TYPE( fc::log_messages ); \
    TYPE( const TYPE& c ); \
    TYPE(); \
    virtual const char* what() const throw(); \
    int32_t get_rpc_error_code() const; \
  };


RPC_DECLARE_EXCEPTION(rpc_client_not_connected_exception)

RPC_DECLARE_EXCEPTION(rpc_wallet_unlock_needed_exception)
RPC_DECLARE_EXCEPTION(rpc_wallet_passphrase_incorrect_exception)

RPC_DECLARE_EXCEPTION(rpc_wallet_open_needed_exception)

} } // bts::rpc

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::rpc::rpc_server::config, (rpc_user)(rpc_password)(rpc_endpoint) )
