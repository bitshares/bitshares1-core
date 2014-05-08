#include <bts/rpc/rpc_server.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>

#include <fc/interprocess/file_mapping.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/server.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>

#include <iomanip>
#include <limits>
#include <sstream>

namespace bts { namespace rpc {

#define RPC_METHOD_LIST\
             (help)\
             (getinfo)\
             (open_wallet)\
             (create_wallet)\
             (current_wallet)\
             (close_wallet)\
             (walletlock)\
             (walletpassphrase)\
             (getnewaddress)\
             (add_send_address)\
             (_create_sendtoaddress_transaction)\
             (_send_transaction)\
             (sendtoaddress)\
             (list_send_addresses)\
             (list_receive_addresses)\
             (get_send_address_label)\
             (getbalance)\
             (getblockhash)\
             (getblockcount)\
             (gettransaction)\
             (get_transaction_history)\
             (getblock)\
             (get_block_by_number)\
             (get_name_record)\
             (validateaddress)\
             (rescan)\
             (import_wallet)\
             (import_private_key)\
             (importprivkey)\
             (set_receive_address_memo)\
             (getconnectioncount)\
             (get_delegates)\
             (reserve_name)\
             (register_delegate)\
             (get_names)\
             (getpeerinfo)\
             (_set_advanced_node_parameters)\
             (addnode)\
             (stop)\
             (_get_transaction_first_seen_time)\
             (_get_block_first_seen_time)

  namespace detail
  {
    class rpc_server_impl
    {
       public:
         rpc_server::config  _config;
         client_ptr          _client;
         fc::http::server    _httpd;
         fc::tcp_server      _tcp_serv;
         fc::future<void>    _accept_loop_complete;
         rpc_server*         _self;

         typedef std::map<std::string, rpc_server::method_data> method_map_type;
         method_map_type _method_map;

         /** the set of connections that have successfully logged in */
         std::unordered_set<fc::rpc::json_connection*> _authenticated_connection_set;

         std::string make_short_description(const rpc_server::method_data& method_data)
         {
           std::string help_string;
           std::stringstream sstream;
           //format into columns
           sstream << std::setw(70) << std::left;
           help_string = method_data.name + " ";
           for (const rpc_server::parameter_data& parameter : method_data.parameters)
           {
             if (parameter.required)
               help_string += std::string("<") + parameter.name + std::string("> ");
             else
               help_string += std::string("[") + parameter.name + std::string("] ");
           }
           sstream << help_string << "  " << method_data.description << "\n";
           help_string = sstream.str();
           return help_string;
         }

         void handle_request( const fc::http::request& r, const fc::http::server::response& s )
         {
             s.add_header( "Connection", "close" );
             // ilog( "handle request ${r}", ("r",r.path) );

             try {
                if( _config.rpc_user.size() )
                {
                   auto auth_value = r.get_header( "Authorization" );
                   std::string username, password;
                   if( auth_value.size() )
                   {
                      auto auth_b64    = auth_value.substr( 6 );
                      auto userpass    = fc::base64_decode( auth_b64 );
                      auto split       = userpass.find( ':' );
                      username    = userpass.substr( 0, split );
                      password    = userpass.substr( split + 1 );
                   }
                   // ilog( "username: '${u}' password: '${p}' config ${c}", ("u",username)("p",password)("c",_config) );
                   if( _config.rpc_user     != username ||
                       _config.rpc_password != password )
                   {
                      s.add_header( "WWW-Authenticate", "Basic realm=\"bts wallet\"" );
                      std::string message = "Unauthorized";
                      s.set_length( message.size() );
                      s.set_status( fc::http::reply::NotAuthorized );
                      s.write( message.c_str(), message.size() );
                   }
                }

                auto dotpos = r.path.find( ".." );
                FC_ASSERT( dotpos == std::string::npos );
                auto filename = _config.htdocs / r.path.substr(1,std::string::npos);
                if( r.path == "/" )
                {
                    filename = _config.htdocs / "index.html";
                }
                if( fc::exists( filename ) )
                {
                    FC_ASSERT( !fc::is_directory( filename ) );
                    uint64_t file_size = fc::file_size( filename );
                    FC_ASSERT( file_size != 0 );
                    FC_ASSERT(file_size <= std::numeric_limits<size_t>::max());

                    fc::file_mapping fm( filename.generic_string().c_str(), fc::read_only );
                    fc::mapped_region mr( fm, fc::read_only, 0, fc::file_size( filename ) );

                    s.set_status( fc::http::reply::OK );
                    s.set_length( file_size );
                    s.write( (const char*)mr.get_address(), mr.get_size() );
                    return;
                }
                if( r.path == fc::path("/rpc") )
                {
                   handle_http_rpc( r, s );
                   return;
                }
                filename = _config.htdocs / "404.html";
                FC_ASSERT( !fc::is_directory( filename ) );
                auto file_size = fc::file_size( filename );
                FC_ASSERT( file_size != 0 );

                fc::file_mapping fm( filename.generic_string().c_str(), fc::read_only );
                fc::mapped_region mr( fm, fc::read_only, 0, fc::file_size( filename ) );

                s.set_status( fc::http::reply::OK );
                s.set_length( file_size );
                s.write( (const char*)mr.get_address(), mr.get_size() );
             }
             catch ( const fc::exception& e )
             {
                    std::string message = "Internal Server Error\n";
                    message += e.to_detail_string();
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::InternalServerError );
                    s.write( message.c_str(), message.size() );
                    elog( "${e}", ("e",e.to_detail_string() ) );
             }
             catch ( ... )
             {
                    std::string message = "Invalid RPC Request\n";
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::BadRequest );
                    s.write( message.c_str(), message.size() );
                    ilog( "${e}", ("e",message) );
             }

         }

         void handle_http_rpc(const fc::http::request& r, const fc::http::server::response& s )
         {
                std::string str(r.body.data(),r.body.size());
                try {
                   auto rpc_call = fc::json::from_string( str ).get_object();
                   auto method_name = rpc_call["method"].as_string();
                   auto params = rpc_call["params"].get_array();

                   // ilog( "method: ${m}  params: ${p}", ("m", method_name)("p",params) );
                   ilog( "call: ${c}", ("c", str) );

                   auto call_itr = _method_map.find( method_name );
                   if( call_itr != _method_map.end() )
                   {
                      fc::mutable_variant_object  result;
                      result["id"]     =  rpc_call["id"];
                      try
                      {
                         result["result"] = dispatch_authenticated_method(call_itr->second, params);
                         auto reply = fc::json::to_string( result );
                         s.set_status( fc::http::reply::OK );
                      }
                      catch ( const fc::exception& e )
                      {
                         s.set_status( fc::http::reply::InternalServerError );
                         result["error"] = fc::mutable_variant_object( "message",e.to_detail_string() );
                      }
                      ilog( "${e}", ("e",result) );
                      auto reply = fc::json::to_string( result );
                      s.set_length( reply.size() );
                      s.write( reply.c_str(), reply.size() );
                      return;
                   }
                   else
                   {
                       std::string message = "Invalid Method: " + method_name;
                       fc::mutable_variant_object  result;
                       result["id"]     =  rpc_call["id"];
                       s.set_status( fc::http::reply::NotFound );
                       result["error"] = fc::mutable_variant_object( "message", message );

                       ilog( "${e}", ("e",result) );
                       auto reply = fc::json::to_string( result );
                       s.set_length( reply.size() );
                       s.write( reply.c_str(), reply.size() );
                       return;
                   }
                }
                catch ( const fc::exception& e )
                {
                    std::string message = "Invalid RPC Request\n";
                    message += e.to_detail_string();
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::BadRequest );
                    s.write( message.c_str(), message.size() );
                    elog( "${e}", ("e",e.to_detail_string() ) );
                }
                catch ( const std::exception& e )
                {
                    std::string message = "Invalid RPC Request\n";
                    message += e.what();
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::BadRequest );
                    s.write( message.c_str(), message.size() );
                    elog( "${e}", ("e",message) );
                }
                catch (...)
                {
                    std::string message = "Invalid RPC Request\n";
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::BadRequest );
                    s.write( message.c_str(), message.size() );
                    elog( "${e}", ("e",message) );
                }
         }


         void accept_loop()
         {
           while( !_accept_loop_complete.canceled() )
           {
              fc::tcp_socket_ptr sock = std::make_shared<fc::tcp_socket>();
              try
              {
                _tcp_serv.accept( *sock );
              }
              catch ( const fc::exception& e )
              {
                elog( "fatal: error opening socket for rpc connection: ${e}", ("e", e.to_detail_string() ) );
                continue;
              }

              auto buf_istream = std::make_shared<fc::buffered_istream>( sock );
              auto buf_ostream = std::make_shared<fc::buffered_ostream>( sock );

              auto json_con = std::make_shared<fc::rpc::json_connection>( std::move(buf_istream),
                                                                          std::move(buf_ostream) );
              register_methods( json_con );

         //   TODO  0.5 BTC: handle connection errors and and connection closed without
         //   creating an entirely new context... this is waistful
         //     json_con->exec();
              fc::async( [json_con]{ json_con->exec().wait(); } );
           }
         }

         void register_methods( const fc::rpc::json_connection_ptr& con )
         {
            ilog( "login!" );
            fc::rpc::json_connection* capture_con = con.get();

            // the login method is a special case that is only used for raw json connections
            // (not for the CLI or HTTP(s) json rpc)
            con->add_method("login", boost::bind(&rpc_server_impl::login, this, capture_con, _1));
            for (const method_map_type::value_type& method : _method_map)
            {
              con->add_method(method.first, boost::bind(&rpc_server_impl::dispatch_method_from_json_connection,
                                                        this, capture_con, method.second, _1));
            }
         } // register methods

        fc::variant dispatch_method_from_json_connection(fc::rpc::json_connection* con,
                                                         const rpc_server::method_data& method_data,
                                                         const fc::variants& arguments)
        {
          // ilog( "arguments: ${params}", ("params",arguments) );
          if ((method_data.prerequisites & rpc_server::json_authenticated) &&
              _authenticated_connection_set.find(con) == _authenticated_connection_set.end())
            FC_THROW_EXCEPTION(exception, "not logged in");
          return dispatch_authenticated_method(method_data, arguments);
        }

        fc::variant dispatch_authenticated_method(const rpc_server::method_data& method_data,
                                                  const fc::variants& arguments)
        {
          // ilog( "arguments: ${params}", ("params",arguments) );
          if (method_data.prerequisites & rpc_server::wallet_open)
            check_wallet_is_open();
          if (method_data.prerequisites & rpc_server::wallet_unlocked)
            check_wallet_unlocked();
          if (method_data.prerequisites & rpc_server::connected_to_network)
            check_connected_to_network();
          if (arguments.size() > method_data.parameters.size())
            FC_THROW_EXCEPTION(exception, "too many arguments (expected at most ${count})",
                                          ("count", method_data.parameters.size()));
          uint32_t required_argument_count = 0;
          for (const rpc_server::parameter_data& parameter : method_data.parameters)
          {
            if (parameter.required)
              ++required_argument_count;
            else
              break;
          }
          if (arguments.size() < required_argument_count)
            FC_THROW_EXCEPTION(exception, "too few arguments (expected at least ${count})", ("count", required_argument_count));

          return method_data.method(arguments);
        }

        // This method invokes the function directly, called by the CLI intepreter.
        fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
        {
          // ilog( "method: ${method} arguments: ${params}", ("method",method_name)("params",arguments) );
          auto iter = _method_map.find(method_name);
          if (iter == _method_map.end())
            FC_THROW_EXCEPTION(exception, "Invalid command ${command}", ("command", method_name));
          return dispatch_authenticated_method(iter->second, arguments);
        }

        void check_connected_to_network()
        {
          if (!_client->is_connected())
            throw rpc_client_not_connected_exception(FC_LOG_MESSAGE(error, "The client must be connected to the network to execute this command"));
        }

        void check_json_connection_authenticated( fc::rpc::json_connection* con )
        {
          if( con && _authenticated_connection_set.find( con ) == _authenticated_connection_set.end() )
          {
            FC_THROW_EXCEPTION( exception, "not logged in" );
          }
        }

        void check_wallet_unlocked()
        {
          if (_client->get_wallet()->is_locked())
            throw rpc_wallet_unlock_needed_exception(FC_LOG_MESSAGE(error, "The wallet's spending key must be unlocked before executing this command"));
        }

        void check_wallet_is_open()
        {
          if (!_client->get_wallet()->is_open())
            throw rpc_wallet_open_needed_exception(FC_LOG_MESSAGE(error, "The wallet must be open before executing this command"));
        }

        fc::variant login( fc::rpc::json_connection* json_connection, const fc::variants& params );


#define DECLARE_RPC_METHOD( r, visitor, elem )  fc::variant elem( const fc::variants& );
#define DECLARE_RPC_METHODS( METHODS ) BOOST_PP_SEQ_FOR_EACH( DECLARE_RPC_METHOD, v, METHODS )
        DECLARE_RPC_METHODS( RPC_METHOD_LIST )
 #undef DECLARE_RPC_METHOD
 #undef DECLARE_RPC_METHODS
    };

    //register_method(method_data{"login", JSON_METHOD_IMPL(login),
    //          /* description */ "authenticate JSON-RPC connection",
    //          /* returns: */    "bool",
    //          /* params:          name            type       required */
    //                            {{"username", "string",  true},
    //                             {"password", "string",  true}},
    //        /* prerequisites */ 0});
    fc::variant rpc_server_impl::login(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      FC_ASSERT( params.size() == 2 );
      FC_ASSERT( params[0].as_string() == _config.rpc_user )
      FC_ASSERT( params[1].as_string() == _config.rpc_password )
      _authenticated_connection_set.insert( json_connection );
      return fc::variant( true );
    }

    static rpc_server::method_data help_metadata{"help", nullptr,
        /* description */ "Lists commands or detailed help for specified command.",
        /* returns: */    "bool",
        /* params:     */{ { "command", "string", false } },
      /* prerequisites */ rpc_server::no_prerequisites,
      R"(
       )"};
    fc::variant rpc_server_impl::help(const fc::variants& params)
    {
      std::string help_string;
      if (params.size() == 0) //if no arguments, display list of commands with short description
      {
        for (const method_map_type::value_type& method_data_pair : _method_map)
        {
          if (method_data_pair.second.name[0] != '_') // hide undocumented commands
          {
            help_string += make_short_description(method_data_pair.second);
          }
        }
      }
      else if (params.size() == 1 && !params[0].is_null() && !params[0].as_string().empty())
      { //display detailed description of requested command
        std::string command = params[0].as_string();
        auto itr = _method_map.find(command);
        if (itr != _method_map.end())
        {
          rpc_server::method_data method_data = itr->second;
          help_string = make_short_description(method_data);
          help_string += method_data.detailed_description;
        }
        else
        {
          throw; //TODO figure out how to format an error msg here
        }
      }

      boost::algorithm::trim(help_string);
      return fc::variant(help_string);
    }

    static rpc_server::method_data getinfo_metadata{"getinfo", nullptr,
                                     /* description */ "Provides common data, such as balance, block count, connections, and lock time",
                                     /* returns: */    "info",
                                     /* params:          name                 type      required */
                                                       { },
                                   /* prerequisites */ 0} ;
    fc::variant rpc_server_impl::getinfo(const fc::variants& params)
    {
       fc::mutable_variant_object info;
       info["balance"]          = _client->get_wallet()->get_balance(0).get_rounded_amount();
       info["version"]          = BTS_BLOCKCHAIN_VERSION;
       info["protocolversion"]  = BTS_NET_PROTOCOL_VERSION;
       info["walletversion"]    = BTS_WALLET_VERSION;
       info["blocks"]           = _client->get_chain()->head_block_num();
       info["connections"]      = 0;
       info["unlocked_until"]   = 0;
       return fc::variant( std::move(info) );
    }

    static rpc_server::method_data getblockhash_metadata{"getblockhash", nullptr,
                                     /* description */ "Returns hash of block in best-block-chain at index provided..",
                                     /* returns: */    "block_id_type",
                                     /* params:          name                 type      required */
                                                       { {"block_num", "int", true} },
                                      /* prerequisites */ rpc_server::no_prerequisites,
                                      R"(
Arguments:
1. index (numeric, required) The block index

Result:
"hash" (string) The block hash

Examples:
> bitshares-cli getblockhash 1000
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getblockhash", "params": [1000] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
    )" };
    fc::variant rpc_server_impl::getblockhash(const fc::variants& params)
    {
      return fc::variant(_client->get_chain()->fetch_block( params[0].as_int64() ).id());
    }


    static rpc_server::method_data getblockcount_metadata{"getblockcount", nullptr,
                                     /* description */ "Returns the number of blocks in the longest block chain.",
                                     /* returns: */    "int",
                                     /* params:          name                 type      required */
                                                       { },
                                   /* prerequisites */ rpc_server::no_prerequisites,
         R"(
Result:
n (numeric) The current block count

Examples:
> bitshares-cli getblockcount
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getblockcount", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
         )" } ;
    fc::variant rpc_server_impl::getblockcount(const fc::variants& params)
    {
      return fc::variant(_client->get_chain()->head_block_num());
    }


    static rpc_server::method_data open_wallet_metadata{"open_wallet", nullptr,
                                     /* description */ "Unlock the wallet with the given passphrase, if no user is specified 'default' will be used.",
                                     /* returns: */    "bool",
                                     /* params:          name                 type      required */
                                                       {{"wallet_username",   "string", false} ,
                                                        {"wallet_passphrase", "string", false}},
                                   /* prerequisites */ rpc_server::json_authenticated,
								   R"(
								   )"};
    fc::variant rpc_server_impl::open_wallet(const fc::variants& params)
    {
      std::string username = "default";
      if (params.size() >= 1 && !params[0].is_null() && !params[0].as_string().empty())
        username = params[0].as_string();

      std::string passphrase;
      if( params.size() >= 2 && !params[1].is_null() )
        passphrase = params[1].as_string();

      try
      {
        _client->get_wallet()->open( username, passphrase );
        return fc::variant(true);
      }
      catch( const fc::exception& e )
      {
        wlog( "${e}", ("e",e.to_detail_string() ) );
        throw e;
      }
      catch (...) // TODO: this is an invalid conversion to rpc_wallet_passphrase exception...
      {           //       if the problem is 'file not found' or 'invalid user' or 'permission denined'
                  //       or some other filesystem error then it should be properly reported.
        throw rpc_wallet_passphrase_incorrect_exception();
      }
    }

    static rpc_server::method_data create_wallet_metadata{"create_wallet", nullptr,
                                       /* description */ "Create a wallet with the given passphrases",
                                       /* returns: */    "bool",
                                       /* params:          name                   type      required */
                                                         {
                                                          {"username",     "string", true},
                                                          {"wallet_pass",   "string", true},
                                                          {"spending_pass", "string", true}
                                                         },
                                     /* prerequisites */ rpc_server::json_authenticated,
									 R"(
									 )" };
    fc::variant rpc_server_impl::create_wallet(const fc::variants& params)
    {
      std::string username = "default";
      std::string passphrase;
      std::string keypassword;

      if( !params[0].is_null() && !params[0].as_string().empty() )
        username = params[0].as_string();
      if( !params[1].is_null() )
        passphrase = params[1].as_string();
      if( !params[2].is_null() )
        keypassword = params[2].as_string();

      try
      {
        _client->get_wallet()->create( username,
                                       passphrase,
                                       keypassword );
        return fc::variant(true);
      }
      catch (...) // TODO: this is an invalid conversion to rpc_wallet_passphrase exception...
      {           //       if the problem is 'file not found' or 'invalid user' or 'permission denined'
                  //       or some other filesystem error then it should be properly reported.
        throw rpc_wallet_passphrase_incorrect_exception();
      }
    }

    static rpc_server::method_data current_wallet_metadata{"current_wallet", nullptr,
                                        /* description */ "Returns the username passed to open_wallet",
                                        /* returns: */    "string",
                                        /* params:     */ {},
                                        /* prerequisites */ rpc_server::no_prerequisites,
									  R"(
									  )" };
    fc::variant rpc_server_impl::current_wallet(const fc::variants& params)
    {
       if( !_client->get_wallet()->is_open() )
          return fc::variant(nullptr);
       return fc::variant(_client->get_wallet()->get_current_user());
    }

    static rpc_server::method_data close_wallet_metadata{"close_wallet", nullptr,
                                      /* description */ "Closes the curent wallet if one is open.",
                                      /* returns: */    "bool",
                                      /* params:     */ {},
                                      /* prerequisites */ rpc_server::no_prerequisites,
									R"(
									)" };
    fc::variant rpc_server_impl::close_wallet(const fc::variants& params)
    {
       return fc::variant( _client->get_wallet()->close() );
    }

    static rpc_server::method_data walletlock_metadata{"walletlock", nullptr,
                                     /* description */ "Lock the private keys in wallet, disables spending commands until unlocked",
                                     /* returns: */    "void",
                                     /* params:     */ {},
                                   /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open};
    fc::variant rpc_server_impl::walletlock(const fc::variants& params)
    {
       try
       {
         _client->get_wallet()->lock_wallet();
         return fc::variant();
       }
       catch (...)
       {
         throw rpc_wallet_passphrase_incorrect_exception();
       }
    }

    static rpc_server::method_data walletpassphrase_metadata{"walletpassphrase", nullptr,
          /* description */ "Unlock the private keys in the wallet to enable spending operations",
          /* returns: */    "void",
          /* params:          name                   type      required */
                            {{"spending_pass", "string", true},
                            {"timeout",             "int",    true} },
        /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
Stores the wallet decryption key in memory for 'timeout' seconds.
This is needed prior to performing transactions related to private keys such as sending bitcoins

Arguments:
1. "spending_pass" (string, required) The wallet spending passphrase
2. timeout (numeric, required) The time to keep the decryption key in seconds.

Note:
Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock
time that overrides the old one if the new time is longer than the old one, but you can immediately
lock the wallet with the walletlock command.

Examples:

unlock the wallet for 60 seconds
> bitshares-cli walletpassphrase "my pass phrase" 60

Lock the wallet again (before 60 seconds)
> bitshares-cli walletlock

As json rpc call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "walletpassphrase", "params": ["my pass phrase", 60] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )"};
    fc::variant rpc_server_impl::walletpassphrase(const fc::variants& params)
    {
       std::string passphrase = params[0].as_string();
       uint32_t timeout_sec = (uint32_t)params[1].as_uint64();
       try
       {
         _client->get_wallet()->unlock_wallet(passphrase, fc::seconds(timeout_sec));
         return fc::variant();
       }
       catch (...)
       {
         throw rpc_wallet_passphrase_incorrect_exception();
       }
    }

    static rpc_server::method_data add_send_address_metadata{"add_send_address", nullptr,
            /* description */ "Add new address for sending payments",
            /* returns: */    "bool",
            /* params:          name       type       required */
                              {{"address", "address", true},
                              {"label",   "string",  true}  },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
     )"};
    fc::variant rpc_server_impl::add_send_address( const fc::variants& params )
    {
       auto foreign_address = params[0].as<address>();
       auto label = params[1].as_string();

       _client->get_wallet()->add_send_address( foreign_address, label );
       return fc::variant(true);
    }

    static rpc_server::method_data getnewaddress_metadata{"getnewaddress", nullptr,
          /* description */ "Create a new address for receiving payments",
          /* returns: */    "address",
          /* params:          name       type      required */
                            {{"account", "string", false},},
        /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
     R"(
getnewaddress ( "account" )

Returns a new BitShares address for receiving payments.
If 'account' is specified (recommended), it is added to the address book
so payments received with the address will be credited to 'account'.

Arguments:
1. "account" (string, optional) The account name for the address to be linked to. if not provided, the default account "" is used. It can also be set to the empty string "" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.

Result:
"bitsharesaddress" (string) The new BitShares address

Examples:
> bitshares-cli getnewaddress
> bitshares-cli getnewaddress ""
> bitshares-cli getnewaddress "myaccount"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getnewaddress", "params": ["myaccount"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::getnewaddress(const fc::variants& params)
    {
       std::string account;
       if (params.size() == 1)
         account = params[0].as_string();
       bts::blockchain::address new_address = _client->get_wallet()->new_receive_address(account);
       return fc::variant(new_address);
    }



    static rpc_server::method_data _create_sendtoaddress_transaction_metadata{"_create_sendtoaddress_transaction", nullptr,
          /* description */ "Creates a transaction in the same manner as 'sendtoaddress', but do not broadcast it",
          /* returns: */    "signed_transaction",
          /* params:          name          type       required */
                            {{"to_address", "address", true},
                              {"amount",     "int64",   true},
                              {"comment",    "string",  false},
                              {"to_comment", "string",  false}},
        /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
        R"(
     )" };
    fc::variant rpc_server_impl::_create_sendtoaddress_transaction(const fc::variants& params)
    {
       bts::blockchain::address destination_address = params[0].as<bts::blockchain::address>();
       auto amount = params[1].as<int64_t>();
       std::string comment;
       if (params.size() >= 3)
         comment = params[2].as_string();
       // TODO: we're currently ignoring optional parameter 4, [to-comment]
       return fc::variant(_client->get_wallet()->send_to_address(asset(amount), destination_address, comment));
    }

    static rpc_server::method_data _send_transaction_metadata{"_send_transaction", nullptr,
            /* description */ "Broadcast a previously-created signed transaction to the network",
            /* returns: */    "transaction_id",
            /* params:          name                  type                   required */
                              {{"signed_transaction", "signed_transaction",  true}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::connected_to_network,
          R"(
     )" };
    fc::variant rpc_server_impl::_send_transaction(const fc::variants& params)
    {
      bts::blockchain::signed_transaction transaction = params[0].as<bts::blockchain::signed_transaction>();
      _client->broadcast_transaction(transaction);
      return fc::variant(transaction.id());
    }

    static rpc_server::method_data sendtoaddress_metadata{"sendtoaddress", nullptr,
            /* description */ "Sends given amount to the given address, assumes shares in DAC",
            /* returns: */    "transaction_id",
            /* params:          name          type       required */
                              {{"to_address", "address", true},
                                {"amount",     "int64",   true},
                                {"comment",    "string",  false},
                                {"to_comment", "string",  false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
sendtoaddress "bitsharesaddress" amount ( "comment" "comment-to" )

Arguments:
1. "to_address" (string, required) The address to send to.
2. "amount" (numeric, required) The amount in BTS to send. eg 10
3. "comment" (string, optional) A comment used to store what the transaction is for.
This is not part of the transaction, just kept in your wallet.
4. "to_comment" (string, optional) A comment to store the name of the person or organization
to which you're sending the transaction. This is not part of the
transaction, just kept in your wallet (TODO: ignored currently, implement later).

Result:
"transactionid" (string) The transaction id. (view at https://????/tx/[transactionid])

Examples:
> bitshares-cli sendtoaddress "1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd" 10
> bitshares-cli sendtoaddress "1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd" 10 "donation" "seans outpost"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "sendtoaddress", "params": ["1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd", 10, "donation", "seans outpost"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::sendtoaddress(const fc::variants& params)
    {
      bts::blockchain::address destination_address = params[0].as<bts::blockchain::address>();
      auto amount = params[1].as_uint64();
      std::string comment;
      if (params.size() >= 3)
        comment = params[2].as_string();
      // TODO: we're currently ignoring optional 4, [to-comment]
      bts::blockchain::signed_transaction trx = _client->get_wallet()->send_to_address( asset(amount,0), destination_address, comment);
      _client->broadcast_transaction(trx);
      return fc::variant( trx.id() );
    }

    static rpc_server::method_data list_receive_addresses_metadata{"list_receive_addresses", nullptr,
            /* description */ "Lists all receive addresses and their labels associated with this wallet",
            /* returns: */    "set<receive_address>",
            /* params:     */ {},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::list_receive_addresses(const fc::variants& params)
    {
      std::unordered_set<bts::wallet::receive_address> addresses = _client->get_wallet()->get_receive_addresses();
      return fc::variant( addresses );
    }

    static rpc_server::method_data get_send_address_label_metadata{"get_send_address_label", nullptr,
                                                 /* description */ "Looks up the label for a single send address, returns null if not found",
                                                 /* returns: */    "string",
                                                /* params:            name          type       required */
                                                                   { {"address",    "address", true} },
                                               /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open};
    fc::variant rpc_server_impl::get_send_address_label( const fc::variants& params )
    {
      std::unordered_map<bts::blockchain::address,std::string> addresses = _client->get_wallet()->get_send_addresses();
      auto itr = addresses.find( params[0].as<address>() );
      if( itr == addresses.end() ) return fc::variant();
      return fc::variant(itr->second);
    }

    static rpc_server::method_data list_send_addresses_metadata{"list_send_addresses", nullptr,
            /* description */ "Lists all foreign addresses and their labels associated with this wallet",
            /* returns: */    "map<address,string>",
            /* params:     */ {},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::list_send_addresses(const fc::variants& params)
    {
      std::unordered_map<bts::blockchain::address,std::string> addresses = _client->get_wallet()->get_send_addresses();
      return fc::variant( addresses );
    }

    static rpc_server::method_data getbalance_metadata{"getbalance", nullptr,
            /* description */ "Returns the wallet's current balance",
            /* returns: */    "asset",
            /* params:          name     type     required */
                              {{"asset", "unit",  false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
TODO: HOW SHOULD THIS BEHAVE WITH ASSETS AND ACCOUNTS?
getbalance ( "account" min_confirms )

If account is not specified, returns the wallet's total available balance.
If account is specified, returns the balance in the account.
Note that the account "" is not the same as leaving the parameter out.
The wallet total may be different to the balance in the default "" account.

Arguments:
1. "account" (string, optional) The selected account, or "*" for entire wallet. It may be the default account using "".
2. min_confirms (numeric, optional, default=1) Only include transactions confirmed at least this many times.

Result:
amount (numeric) The total amount in BTS received for this account.

Examples:

The total amount in the server across all accounts
> bitshares-cli getbalance

The total amount in the server across all accounts, with at least 5 confirmations
> bitshares-cli getbalance "*" 6

The total amount in the default account with at least 1 confirmation
> bitshares-cli getbalance ""

The total amount in the account named tabby with at least 6 confirmations
> bitshares-cli getbalance "tabby" 6

As a json rpc call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getbalance", "params": ["tabby", 6] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::getbalance(const fc::variants& params)
    {
      bts::blockchain::asset_type unit = 0;
      if (params.size() == 1)
        unit = params[0].as<bts::blockchain::asset_type>();
      return fc::variant( _client->get_wallet()->get_balance( unit ) );
    }

    static rpc_server::method_data set_receive_address_memo_metadata{"set_receive_address_memo", nullptr,
            /* description */ "Retrieves all transactions into or out of this wallet.",
            /* returns: */    "map<transaction_id,transaction_state>",
            /* params:          name     type     required */
                              {{"address", "address",  true},
                               {"memo", "string",  true}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
     )" };
    fc::variant rpc_server_impl::set_receive_address_memo(const fc::variants& params)
    {
      _client->get_wallet()->set_receive_address_memo(params[0].as<address>(), params[1].as_string());
      return fc::variant();
    }

    static rpc_server::method_data get_transaction_history_metadata{"get_transaction_history", nullptr,
            /* description */ "Retrieves all transactions into or out of this wallet.",
            /* returns: */    "std::vector<transaction_state>",
            /* params:          name     type     required */
                              {{"count", "unsigned",  false}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
     )" };
    fc::variant rpc_server_impl::get_transaction_history(const fc::variants& params)
    {
      unsigned count = 0;
      if (params.size() == 1)
          count = params[0].as<unsigned>();

      return fc::variant( _client->get_wallet()->get_transaction_history( count ) );
    }

    static rpc_server::method_data get_name_record_metadata{"get_name_record", nullptr,
            /* description */ "Retrieves the name record",
            /* returns: */    "name_record",
            /* params:          name              type               required */
                             {{"name",          "string",            true}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
     )" };
    fc::variant rpc_server_impl::get_name_record(const fc::variants& params)
    {
      return fc::variant( _client->get_chain()->lookup_name(params[0].as_string()) );
    }
    static rpc_server::method_data reserve_name_metadata{"reserve_name", nullptr,
            /* description */ "Retrieves the name record",
            /* returns: */    "name_record",
            /* params:          name              type               required */
                             {{"name",          "string",            true},
                              {"data",          "variant",           true}},
            /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
     )" };
    fc::variant rpc_server_impl::reserve_name(const fc::variants& params)
    {
       return fc::variant( _client->reserve_name(params[0].as_string(), params[1]) );
    }
    static rpc_server::method_data register_delegate_metadata{"register_delegate", nullptr,
            /* description */ "Registeres a delegate to be voted upon by shareholders.",
            /* returns: */    "name_record",
            /* params:          name              type               required */
                             {{"name",          "string",            true},
                              {"data",          "variant",           true}},
            /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
     )" };
    fc::variant rpc_server_impl::register_delegate(const fc::variants& params)
    {
       return fc::variant( _client->register_delegate(params[0].as_string(), params[1]) );
    }

    static rpc_server::method_data gettransaction_metadata{"gettransaction", nullptr,
            /* description */ "Get detailed information about an in-wallet transaction",
            /* returns: */    "signed_transaction",
            /* params:          name              type               required */
                              {{"transaction_id", "transaction_id",  true}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
Arguments:
1. "transaction_id" (string, required) The transaction id

Result:
{
"amount" : xx, (numeric) The transaction amount in BTS
"confirmations" : n, (numeric) The number of confirmations
"blockhash" : "hash", (string) The block hash
"blockindex" : xx, (numeric) The block index
"blocktime" : ttt, (numeric) The time in seconds since epoch (1 Jan 1970 GMT)
"txid" : "transactionid", (string) The transaction id, see also https://???.info/tx/[transactionid]
"time" : ttt, (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)
"timereceived" : ttt, (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)
"details" : [
{
"account" : "accountname", (string) The account name involved in the transaction, can be "" for the default account.
"address" : "bitsharesaddress", (string) The BitShares address involved in the transaction
"category" : "send|receive", (string) The category, either 'send' or 'receive'
"amount" : xx (numeric) The amount in BTS
}
,...
],
"hex" : "data" (string) Raw data for transaction
}

bExamples
> bitshares-cli gettransaction "1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "gettransaction", "params": ["1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::gettransaction(const fc::variants& params)
    {
      return fc::variant( _client->get_chain()->fetch_transaction( params[0].as<transaction_id_type>() )  );
    }

    static rpc_server::method_data getblock_metadata{"getblock", nullptr,
            /* description */ "Retrieves the block header for the given block hash",
            /* returns: */    "block_header",
            /* params:          name              type        required */
                              {{"block_hash",   "block_id_type", true}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
getblock "hash" ( verbose )

If verbose is false, returns a string that is serialized, hex-encoded data for block 'hash'.
If verbose is true, returns an Object with information about block <hash>.

Arguments:
1. "hash" (string, required) The block hash
2. verbose (boolean, optional, default=true) true for a json object, false for the hex encoded data

Result (for verbose = true):
{
"hash" : "hash", (string) the block hash (same as provided)
"confirmations" : n, (numeric) The number of confirmations
"size" : n, (numeric) The block size
"height" : n, (numeric) The block height or index
"version" : n, (numeric) The block version
"merkleroot" : "xxxx", (string) The merkle root
"tx" : [ (array of string) The transaction ids
"transactionid" (string) The transaction id
,...
],
"time" : ttt, (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)
"nonce" : n, (numeric) The nonce //TODO: fake this
"bits" : "1d00ffff", (string) The bits
"difficulty" : x.xxx, (numeric) The difficulty //TODO: fake this
"previousblockhash" : "hash", (string) The hash of the previous block
"nextblockhash" : "hash" (string) The hash of the next block
}

Result (for verbose=false):
"data" (string) A string that is serialized, hex-encoded data for block 'hash'.

Examples:
> bitshares-cli getblock "00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getblock", "params": ["00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::getblock(const fc::variants& params)
    { try {
      auto blocknum = _client->get_chain()->fetch_block_num( params[0].as<block_id_type>() );
      return fc::variant( _client->get_chain()->fetch_block( blocknum )  );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data get_block_by_number_metadata{"get_block_by_number", nullptr,
                                   /* description */ "Retrieves the block header for the given block number",
                                   /* returns: */    "block_header",
                                   /* params:          name              type        required */
                                                     {{"block_number",   "int32", true}},
                                 /* prerequisites */ rpc_server::json_authenticated};
    fc::variant rpc_server_impl::get_block_by_number(const fc::variants& params)
    { try {
      return fc::variant( _client->get_chain()->fetch_block( params[0].as<uint32_t>() ) );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data validateaddress_metadata{"validateaddress", nullptr,
            /* description */ "Return information about given BitShares address.",
            /* returns: */    "bool",
            /* params:          name              type       required */
                              {{"address",        "address", true}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
TODO: bitcoin supports all below info

Arguments:
1. "bitsharesaddress" (string, required) The BitShares address to validate

Result:
{
"isvalid" : true|false, (boolean) If the address is valid or not. If not, this is the only property returned.
"address" : "bitsharesaddress", (string) The BitShares address validated
"ismine" : true|false, (boolean) If the address is yours or not
"isscript" : true|false, (boolean) If the key is a script
"pubkey" : "publickeyhex", (string) The hex value of the raw public key
"iscompressed" : true|false, (boolean) If the address is compressed
"account" : "account" (string) The account associated with the address, "" is the default account
}

Examples:
> bitshares-cli validateaddress "1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "validateaddress", "params": ["1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::validateaddress(const fc::variants& params)
    {
      try {
        return fc::variant(params[0].as_string());
      }
      catch ( const fc::exception& )
      {
        return fc::variant(false);
      }
    }

    static rpc_server::method_data rescan_metadata{"rescan", nullptr,
            /* description */ "Rescan the block chain from the given block",
            /* returns: */    "bool",
            /* params:          name              type    required */
                              {{"starting_block", "bool", false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::rescan(const fc::variants& params)
    {
      uint32_t block_num = 0;
      if (params.size() == 1)
        block_num = (uint32_t)params[0].as_int64();
      _client->get_wallet()->scan_chain(*_client->get_chain(), block_num);
      return fc::variant(true);
    }

    static rpc_server::method_data import_wallet_metadata{"import_wallet", nullptr,
            /* description */ "Import a BTC/PTS wallet",
            /* returns: */    "bool",
            /* params:          name               type       required */
                              {{"filename", "string",  true}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
          R"(
     )" };
    fc::variant rpc_server_impl::import_wallet(const fc::variants& params)
    {
      auto arguments       = params[0].as<std::pair<fc::path, std::string>>();
      auto wallet_dat      = arguments.first;
      auto wallet_password = arguments.second;
      _client->get_wallet()->import_bitcoin_wallet( wallet_dat, wallet_password );
      return fc::variant(true);
    }

    // TODO: get account argument
    static rpc_server::method_data import_private_key_metadata{"import_private_key", nullptr,
            /* description */ "Import a BTC/PTS/BTS private key in hex format",
            /* returns: */    "bool",
            /* params:          name           type            required */
                              {{"key",         "private_key",  true},
                                {"label",       "string",       false},
                                {"rescan",      "bool",         false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
          R"(
     )" };
    fc::variant rpc_server_impl::import_private_key(const fc::variants& params)
    {
      auto key = params[0].as<fc::ecc::private_key>();
      std::string label;
      if (params.size() >= 2 && !params[1].is_null())
        label = params[1].as_string();

      bool rescan = false;
      if (params.size() == 3 && params[2].as_bool())
        rescan = true;

      _client->get_wallet()->import_key(key, label);
      _client->get_wallet()->save();

      if (rescan)
          _client->get_wallet()->scan_chain(*_client->get_chain(), 0);

      return fc::variant(true);
    }

    // TODO: get account argument
    static rpc_server::method_data importprivkey_metadata{"importprivkey", nullptr,
            /* description */ "Import a BTC/PTS private key in wallet import format (WIF)",
            /* returns: */    "bool",
            /* params:          name           type            required */
                              {{"key",         "private_key",  true},
                                {"label",       "string",       false},
                                {"rescan",      "bool",         false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
          R"(
importprivkey "bitcoinprivkey" ( "account" rescan "address_label")

Adds a private key (as returned by dumpprivkey) to your wallet.

Arguments:
1. "bitcoinprivkey" (string, required) The private key (see dumpprivkey)
2. "account" (string, optional) the name of the account to put this key in
3. rescan (boolean, optional, default=true) Rescan the wallet for transactions
4. "address_label" (string,optional) assigns a label to the address being imported

Examples:

Dump a private key from Bitcoin wallet
> bitshares-cli dumpprivkey "myaddress"

Import the private key to BitShares wallet
> bitshares-cli importprivkey "mykey"

Import using an account name
> bitshares-cli importprivkey "mykey" "testing" false "address_label"

As a json rpc call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "importprivkey", "params": ["mykey", "testing", false] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::importprivkey(const fc::variants& params)
    {
      auto wif   =  params[0].as_string();
      auto label =  params[1].as_string();
      bool rescan = false;
      if (params.size() == 3 && params[2].as_bool())
        rescan = true;

      _client->get_wallet()->import_wif_key(wif, label);
      _client->get_wallet()->save();

      if (rescan)
          _client->get_wallet()->scan_chain(*_client->get_chain(), 0);

      return fc::variant(true);
    }

    static rpc_server::method_data get_names_metadata{"get_names", nullptr,
            /* description */ "Returns the list of reserved names sorted alphabetically",
            /* returns: */    "vector<delegate_status>",
            /* params:     */ { {"first", "string", false},
                                {"count", "int", false} },
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
get_names (first, count)

Returns up to count reserved names that follow first alphabetically.
             )" };
    fc::variant rpc_server_impl::get_names(const fc::variants& params)
    {
      return fc::variant(_client->get_chain()->get_names( params[0].as_string(), params[0].as_int64() ) );
    }

    static rpc_server::method_data get_delegates_metadata{"get_delegates", nullptr,
            /* description */ "Returns the list of delegates sorted by vote",
            /* returns: */    "vector<delegate_status>",
            /* params:     */ { {"first", "int", false},
                                {"count", "int", false} },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
get_delegates (start, count)

Returns information about the delegates sorted by their net votes starting from position start and returning up to count items.

Arguments:
   first - the index of the first delegate to be returned
   count - the maximum number of delegates to be returned

             )" };

    fc::variant rpc_server_impl::get_delegates(const fc::variants& params)
    {
      return fc::variant(_client->get_wallet()->get_delegates());
    }

    static rpc_server::method_data getconnectioncount_metadata{"getconnectioncount", nullptr,
            /* description */ "Returns the number of connections to other nodes.",
            /* returns: */    "bool",
            /* params:     */ {},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
Result:
n (numeric) The connection count

Examples:
> bitshares-cli getconnectioncount
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getconnectioncount", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/

)" };
    fc::variant rpc_server_impl::getconnectioncount(const fc::variants&)
    {
      return fc::variant(_client->get_connection_count());
    }

    static rpc_server::method_data getpeerinfo_metadata{"getpeerinfo", nullptr,
            /* description */ "Returns data about each connected node.",
            /* returns: */    "vector<jsonobject>",
            /* params:     */ {},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
getpeerinfo

Returns data about each connected network node as a json array of objects.

bResult:
[
{
"addr":"host:port", (string) The ip address and port of the peer
"addrlocal":"ip:port", (string) local address
"services":"00000001", (string) The services
"lastsend": ttt, (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last send
"lastrecv": ttt, (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last receive
"bytessent": n, (numeric) The total bytes sent
"bytesrecv": n, (numeric) The total bytes received
"conntime": ttt, (numeric) The connection time in seconds since epoch (Jan 1 1970 GMT)
"pingtime": n, (numeric) ping time
"pingwait": n, (numeric) ping wait
"version": v, (numeric) The peer version, such as 7001
"subver": "/Satoshi:0.8.5/", (string) The string version
"inbound": true|false, (boolean) Inbound (true) or Outbound (false)
"startingheight": n, (numeric) The starting height (block) of the peer
"banscore": n, (numeric) The ban score (stats.nMisbehavior)
"syncnode" : true|false (boolean) if sync node
}
,...
}

Examples:
> bitcoin-cli getpeerinfo
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getpeerinfo", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
)" };
    fc::variant rpc_server_impl::getpeerinfo(const fc::variants&)
    {
      return _client->get_peer_info();
    }

    static rpc_server::method_data _set_advanced_node_parameters_metadata{"_set_advanced_node_parameters", nullptr,
            /* description */ "Sets advanced node parameters, used for setting up automated tests",
            /* returns: */    "null",
            /* params:     */ { {"params", "jsonobject", true} },
          /* prerequisites */ rpc_server::json_authenticated,
  R"(
Result:
null
  )" };
    fc::variant rpc_server_impl::_set_advanced_node_parameters(const fc::variants& params)
    {
      _client->set_advanced_node_parameters(params[0].get_object());
      return fc::variant();
    }

    static rpc_server::method_data addnode_metadata{"addnode", nullptr,
            /* description */ "Attempts add or remove <node> from the peer list or try a connection to <node> once",
            /* returns: */    "null",
            /* params:          name            type            required */
                              {{"node",         "string",       true},
                               {"command",      "string",       true}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
addnode "node" "add|remove|onetry"

Attempts add or remove a node from the addnode list.
Or try a connection to a node once.

Arguments:
1. "node" (string, required) The node (see getpeerinfo for nodes)
2. "command" (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the node once

Examples:
> bitcoin-cli addnode "192.168.0.6:8333" "onetry"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "addnode", "params": ["192.168.0.6:8333", "onetry"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
)" };
    fc::variant rpc_server_impl::addnode(const fc::variants& params)
    {
      _client->addnode(fc::ip::endpoint::from_string(params[0].as_string()), params[1].as_string());
      return fc::variant();
    }

    static rpc_server::method_data stop_metadata{"stop", nullptr,
            /* description */ "Stop BitShares server",
            /* returns: */    "null",
            /* params:     */ {},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
stop

Stop BitShares server.
)" };
    fc::variant rpc_server_impl::stop(const fc::variants& params)
    {
      _client->stop();
      return fc::variant();
    }

    static rpc_server::method_data _get_transaction_first_seen_time_metadata{"_get_transaction_first_seen_time", nullptr,
            /* description */ "Returns the time the transaction was first seen by this client",
            /* returns: */    "null",
            /* params:          name              type               required */
                              {{"transaction_id", "transaction_id",  true}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
_get_transaction_first_seen_time <transaction_id>

Returns the time the transaction was first seen by this client.

This interrogates the p2p node's message cache to find out when it first saw this transaction.
The data in the message cache is only kept for a few blocks, so you can only use this to ask
about recent transactions. This is intended to be used to track message propagation delays
in our test network.
)" };
    fc::variant rpc_server_impl::_get_transaction_first_seen_time(const fc::variants& params)
    {
      return fc::variant(_client->get_transaction_first_seen_time(params[0].as<transaction_id_type>()));
    }
    static rpc_server::method_data _get_block_first_seen_time_metadata{"_get_block_first_seen_time", nullptr,
            /* description */ "Returns the time the block was first seen by this client",
            /* returns: */    "null",
            /* params:          name              type        required */
                              {{"block_hash",   "block_id_type", true}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
_get_block_first_seen_time <block_hash>

Returns the time the block was first seen by this client.

This interrogates the p2p node's message cache to find out when it first saw this transaction.
The data in the message cache is only kept for a few blocks, so you can only use this to ask
about recent transactions. This is intended to be used to track message propagation delays
in our test network.
)" };
    fc::variant rpc_server_impl::_get_block_first_seen_time(const fc::variants& params)
    {
      return fc::variant(_client->get_block_first_seen_time(params[0].as<block_id_type>()));
    }
  } // detail

  bool rpc_server::config::is_valid() const
  {
    if (rpc_user.empty())
      return false;
    if (rpc_password.empty())
      return false;
    if (!rpc_endpoint.port())
      return false;
    return true;
  }

  rpc_server::rpc_server() :
    my(new detail::rpc_server_impl)
  {
    my->_self = this;

#define REGISTER_RPC_METHOD( r, visitor, METHODNAME ) \
    do { \
      method_data data_with_functor(detail::BOOST_PP_CAT(METHODNAME,_metadata)); \
      data_with_functor.method = boost::bind(&detail::rpc_server_impl::METHODNAME, my.get(), _1); \
      register_method(data_with_functor); \
    } while (0);
#define REGISTER_RPC_METHODS( METHODS ) \
    BOOST_PP_SEQ_FOR_EACH( REGISTER_RPC_METHOD, v, METHODS )

    REGISTER_RPC_METHODS( RPC_METHOD_LIST )

 #undef REGISTER_RPC_METHODS
 #undef REGISTER_RPC_METHOD
  }

  rpc_server::~rpc_server()
  {
     try {
         my->_tcp_serv.close();
         if( my->_accept_loop_complete.valid() )
         {
            my->_accept_loop_complete.cancel();
            my->_accept_loop_complete.wait();
         }
     }
     catch ( const fc::canceled_exception& ){}
     catch ( const fc::exception& e )
     {
        wlog( "unhandled exception thrown in destructor.\n${e}", ("e", e.to_detail_string() ) );
     }
  }

  client_ptr rpc_server::get_client()const
  {
     return my->_client;
  }

  void rpc_server::set_client( const client_ptr& c )
  {
     my->_client = c;
  }

  void rpc_server::configure( const rpc_server::config& cfg )
  {
    if (!cfg.is_valid())
      return;
    try
    {
      my->_config = cfg;
      my->_tcp_serv.listen( cfg.rpc_endpoint );
      ilog( "listening for json rpc connections on port ${port}", ("port",my->_tcp_serv.get_port()) );

      my->_accept_loop_complete = fc::async( [=]{ my->accept_loop(); } );


      auto m = my.get();
      my->_httpd.listen(cfg.httpd_endpoint);
      my->_httpd.on_request( [m]( const fc::http::request& r, const fc::http::server::response& s ){ m->handle_request( r, s ); } );

    } FC_RETHROW_EXCEPTIONS( warn, "attempting to configure rpc server ${port}", ("port",cfg.rpc_endpoint)("config",cfg) );
  }

  fc::variant rpc_server::direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
  {
    return my->direct_invoke_method(method_name, arguments);
  }

  const rpc_server::method_data& rpc_server::get_method_data(const std::string& method_name)
  {
    auto iter = my->_method_map.find(method_name);
    if (iter != my->_method_map.end())
      return iter->second;
    FC_THROW_EXCEPTION(key_not_found_exception, "Method \"${name}\" not found", ("name", method_name));
  }

  void rpc_server::register_method(method_data data)
  {
    my->_method_map.insert(detail::rpc_server_impl::method_map_type::value_type(data.name, data));
  }

  void rpc_server::check_connected_to_network()
  {
    my->check_connected_to_network();
  }

  void rpc_server::check_wallet_unlocked()
  {
    my->check_wallet_unlocked();
  }

  void rpc_server::check_wallet_is_open()
  {
    my->check_wallet_is_open();
  }

  exception::exception(fc::log_message&& m) :
    fc::exception(fc::move(m)) {}

  exception::exception(){}
  exception::exception(const exception& t) :
    fc::exception(t) {}
  exception::exception(fc::log_messages m) :
    fc::exception() {}

#define RPC_EXCEPTION_IMPL(TYPE, ERROR_CODE, DESCRIPTION) \
  TYPE::TYPE(fc::log_message&& m) : \
    exception(fc::move(m)) {} \
  TYPE::TYPE(){} \
  TYPE::TYPE(const TYPE& t) : exception(t) {} \
  TYPE::TYPE(fc::log_messages m) : \
    exception() {} \
  const char* TYPE::what() const throw() { return DESCRIPTION; } \
  int32_t TYPE::get_rpc_error_code() const { return ERROR_CODE; }

// the codes here match bitcoine error codes in https://github.com/bitcoin/bitcoin/blob/master/src/rpcprotocol.h#L34
RPC_EXCEPTION_IMPL(rpc_misc_error_exception, -1, "std::exception is thrown during command handling")
RPC_EXCEPTION_IMPL(rpc_client_not_connected_exception, -9, "The client is not connected to the network")
RPC_EXCEPTION_IMPL(rpc_wallet_unlock_needed_exception, -13, "The wallet's spending key must be unlocked before executing this command")
RPC_EXCEPTION_IMPL(rpc_wallet_passphrase_incorrect_exception, -14, "The wallet passphrase entered was incorrect")

// these error codes don't match anything in bitcoin
RPC_EXCEPTION_IMPL(rpc_wallet_open_needed_exception, -100, "The wallet must be opened before executing this command")

} } // bts::rpc

