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
             (get_info)\
             (wallet_open_file)\
             (wallet_create)\
             (wallet_open)\
             (wallet_get_name)\
             (wallet_close)\
             (wallet_lock)\
             (wallet_unlock)\
             (wallet_create_receive_account) \
             (wallet_create_sending_account) \
             (wallet_import_private_key)\
             (wallet_import_bitcoin)\
             (wallet_list_sending_accounts)\
             (wallet_list_receive_accounts)\
             (wallet_get_account)\
             (_create_sendtoaddress_transaction)\
             (_send_transaction)\
             (wallet_transfer)\
             (wallet_get_balance)\
             (blockchain_get_blockhash)\
             (blockchain_get_blockcount)\
             (blockchain_get_transaction)\
             (wallet_get_transaction_history)\
             (blockchain_get_block)\
             (blockchain_get_block_by_number)\
             (blockchain_get_name_record)\
             (validate_address)\
             (wallet_rescan_blockchain)\
             (wallet_rescan_blockchain_state)\
             (network_get_connection_count)\
             (blockchain_get_delegates)\
             (wallet_reserve_name)\
             (wallet_register_delegate)\
             (blockchain_get_names)\
             (network_get_peer_info)\
             (_set_advanced_node_parameters)\
             (network_add_node)\
             (stop)\
             (_get_transaction_propagation_data)\
             (_get_block_propagation_data)\
             (_list_json_commands)

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
             fc::time_point begin_time = fc::time_point::now();
             fc_ilog( fc::logger::get("rpc"), "Started ${path} ${method} at ${time}", ("path",r.path)("method",r.method)("time",begin_time));
             fc::http::reply::status_code status = fc::http::reply::OK;

             s.add_header( "Connection", "close" );

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
                   if( _config.rpc_user     != username ||
                       _config.rpc_password != password )
                   {
                      fc_ilog( fc::logger::get("rpc"), "Unauthorized ${path}, username: ${user}", ("path",r.path)("user",username));
                      s.add_header( "WWW-Authenticate", "Basic realm=\"bts wallet\"" );
                      std::string message = "Unauthorized";
                      s.set_length( message.size() );
                      s.set_status( fc::http::reply::NotAuthorized );
                      s.write( message.c_str(), message.size() );
                      return;
                   }
                }

                fc::string path = r.path;
                auto pos = path.find( '?' );
                if( pos != std::string::npos ) path.resize(pos);

                pos = path.find( ".." );
                FC_ASSERT( pos == std::string::npos );

                if( path == "/" ) path = "/index.html";

                auto filename = _config.htdocs / path.substr(1,std::string::npos);
                if( fc::exists( filename ) )
                {
                    FC_ASSERT( !fc::is_directory( filename ) );
                    uint64_t file_size_64 = fc::file_size( filename );
                    FC_ASSERT(file_size_64 <= std::numeric_limits<size_t>::max());
                    size_t file_size = (size_t)file_size_64;
                    FC_ASSERT( file_size != 0 );

                    fc::file_mapping fm( filename.generic_string().c_str(), fc::read_only );
                    fc::mapped_region mr( fm, fc::read_only, 0, file_size );
                    fc_ilog( fc::logger::get("rpc"), "Processing ${path}, size: ${size}", ("path",r.path)("size",file_size));
                    s.set_status( fc::http::reply::OK );
                    s.set_length( file_size );
                    s.write( (const char*)mr.get_address(), mr.get_size() );
                }
                else if( r.path == fc::path("/rpc") )
                {
                    status = handle_http_rpc( r, s );
                }
                else
                {
                    fc_ilog( fc::logger::get("rpc"), "Not found ${path} (${file})", ("path",r.path)("file",filename));
                    filename = _config.htdocs / "404.html";
                    FC_ASSERT( !fc::is_directory( filename ) );
                    uint64_t file_size_64 = fc::file_size( filename );
                    FC_ASSERT(file_size_64 <= std::numeric_limits<size_t>::max());
                    size_t file_size = (size_t)file_size_64;
                    FC_ASSERT( file_size != 0 );

                    fc::file_mapping fm( filename.generic_string().c_str(), fc::read_only );
                    fc::mapped_region mr( fm, fc::read_only, 0, file_size );
                    s.set_status( fc::http::reply::NotFound );
                    s.set_length( file_size );
                    s.write( (const char*)mr.get_address(), mr.get_size() );
                    status = fc::http::reply::NotFound;
                }
             }
             catch ( const fc::exception& e )
             {
                    std::string message = "Internal Server Error\n";
                    message += e.to_detail_string();
                    fc_ilog( fc::logger::get("rpc"), "Internal Server Error ${path} - ${msg}", ("path",r.path)("msg",message));
                    elog("Internal Server Error ${path} - ${msg}", ("path",r.path)("msg",message));
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::InternalServerError );
                    s.write( message.c_str(), message.size() );
                    elog( "${e}", ("e",e.to_detail_string() ) );
                    status = fc::http::reply::InternalServerError;

             }
             catch ( ... )
             {
                    std::string message = "Invalid RPC Request\n";
                    fc_ilog( fc::logger::get("rpc"), "Invalid RPC Request ${path}", ("path",r.path));
                    elog("Invalid RPC Request ${path}", ("path",r.path));
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::BadRequest );
                    s.write( message.c_str(), message.size() );
                    status = fc::http::reply::BadRequest;
             }

             fc::time_point end_time = fc::time_point::now();
             fc_ilog( fc::logger::get("rpc"), "Completed ${path} ${status} in ${ms}ms", ("path",r.path)("status",(int)status)("ms",(end_time - begin_time).count()/1000));
         }

         fc::http::reply::status_code handle_http_rpc(const fc::http::request& r, const fc::http::server::response& s )
         {
                fc::http::reply::status_code status = fc::http::reply::OK;
                std::string str(r.body.data(),r.body.size());
                fc::string method_name;
                try {
                   auto rpc_call = fc::json::from_string( str ).get_object();
                   method_name = rpc_call["method"].as_string();
                   auto params = rpc_call["params"].get_array();
                   auto params_log = fc::json::to_string(rpc_call["params"]);
                   if(method_name.find_first_of("wallet") != std::string::npos || method_name.find_first_of("priv") != std::string::npos)
                       params_log = "***";
                   fc_ilog( fc::logger::get("rpc"), "Processing ${path} ${method} (${params})", ("path",r.path)("method",method_name)("params",params_log));

                   auto call_itr = _method_map.find( method_name );
                   if( call_itr != _method_map.end() )
                   {
                      fc::mutable_variant_object  result;
                      result["id"]     =  rpc_call["id"];
                      try
                      {
                         result["result"] = dispatch_authenticated_method(call_itr->second, params);
                         auto reply = fc::json::to_string( result );
                         status = fc::http::reply::OK;
                         s.set_status( status );
                      }
                      catch ( const fc::exception& e )
                      {
                          status = fc::http::reply::InternalServerError;
                          s.set_status( status );
                          result["error"] = fc::mutable_variant_object( "message",e.to_detail_string() );
                      }
                      ilog( "${e}", ("e",result) );
                      auto reply = fc::json::to_string( result );
                      s.set_length( reply.size() );
                      s.write( reply.c_str(), reply.size() );
                      auto reply_log = reply.size() > 100 ? reply.substr(0,99) + ".." :  reply;
                      fc_ilog( fc::logger::get("rpc"), "Result ${path} ${method}: ${reply}", ("path",r.path)("method",method_name)("reply",reply_log));
                      return status;
                   }
                   else
                   {
                       fc_ilog( fc::logger::get("rpc"), "Invalid Method ${path} ${method}", ("path",r.path)("method",method_name));
                       elog( "Invalid Method ${path} ${method}", ("path",r.path)("method",method_name));
                       std::string message = "Invalid Method: " + method_name;
                       fc::mutable_variant_object  result;
                       result["id"]     =  rpc_call["id"];
                       status = fc::http::reply::NotFound;
                       s.set_status( status );
                       result["error"] = fc::mutable_variant_object( "message", message );
                       auto reply = fc::json::to_string( result );
                       s.set_length( reply.size() );
                       s.write( reply.c_str(), reply.size() );
                       return status;
                   }
                }
                catch ( const fc::exception& e )
                {
                    fc_ilog( fc::logger::get("rpc"), "Invalid RPC Request ${path} ${method}: ${e}", ("path",r.path)("method",method_name)("e",e.to_detail_string()));
                    elog( "Invalid RPC Request ${path} ${method}: ${e}", ("path",r.path)("method",method_name)("e",e.to_detail_string()));
                    std::string message = "Invalid RPC Request\n";
                    message += e.to_detail_string();
                    s.set_length( message.size() );
                    status = fc::http::reply::BadRequest;
                    s.set_status( status );
                    s.write( message.c_str(), message.size() );
                }
                catch ( const std::exception& e )
                {
                    fc_ilog( fc::logger::get("rpc"), "Invalid RPC Request ${path} ${method}: ${e}", ("path",r.path)("method",method_name)("e",e.what()));
                    elog( "Invalid RPC Request ${path} ${method}: ${e}", ("path",r.path)("method",method_name)("e",e.what()));
                    std::string message = "Invalid RPC Request\n";
                    message += e.what();
                    s.set_length( message.size() );
                    status = fc::http::reply::BadRequest;
                    s.set_status( status );
                    s.write( message.c_str(), message.size() );
                }
                catch (...)
                {
                    fc_ilog( fc::logger::get("rpc"), "Invalid RPC Request ${path} ${method} ...", ("path",r.path)("method",method_name));
                    elog( "Invalid RPC Request ${path} ${method} ...", ("path",r.path)("method",method_name));
                    std::string message = "Invalid RPC Request\n";
                    s.set_length( message.size() );
                    status = fc::http::reply::BadRequest;
                    s.set_status( status );
                    s.write( message.c_str(), message.size() );
                }
                return status;
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

          auto result = method_data.method(arguments);


          return result;
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
Arguments:
1. "command" (string, optional) The command to get help on

Result:
"text" (string) The help text
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

    static rpc_server::method_data get_info_metadata{"get_info", nullptr,
                                     /* description */ "Provides common data, such as balance, block count, connections, and lock time",
                                     /* returns: */    "info",
                                     /* params:          name                 type      required */
                                                       { },
                                   /* prerequisites */ 0} ;
    fc::variant rpc_server_impl::get_info(const fc::variants& params)
    {
       fc::mutable_variant_object info;
       if( _client->get_wallet()->is_open() )
       {
          info["balance"]          = _client->get_wallet()->get_balance("*",0).amount;
          info["unlocked_until"]   = _client->get_wallet()->unlocked_until();
       }
       else
       {
          info["balance"]          = 0;
          info["unlocked_until"]   = fc::time_point_sec();
       }
       info["version"]          = BTS_BLOCKCHAIN_VERSION;
       info["protocolversion"]  = BTS_NET_PROTOCOL_VERSION;
       info["walletversion"]    = BTS_WALLET_VERSION;
       info["blocks"]           = _client->get_chain()->get_head_block_num();
       info["connections"]      = _client->get_connection_count();
       info["_node_id"]         = _client->get_node_id();
       return fc::variant( std::move(info) );
    }

    static rpc_server::method_data blockchain_get_blockhash_metadata{"blockchain_get_blockhash", nullptr,
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
> bitshares-cli blockchain_get_blockhash 1000
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "blockchain_get_blockhash", "params": [1000] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
    )" };
    fc::variant rpc_server_impl::blockchain_get_blockhash(const fc::variants& params)
    {
      return fc::variant(_client->get_chain()->get_block( (uint32_t)params[0].as_int64() ).id());
    }


    static rpc_server::method_data blockchain_get_blockcount_metadata{"blockchain_get_blockcount", nullptr,
                                     /* description */ "Returns the number of blocks in the longest block chain.",
                                     /* returns: */    "int",
                                     /* params:          name                 type      required */
                                                       { },
                                   /* prerequisites */ rpc_server::no_prerequisites,
         R"(
Result:
n (numeric) The current block count

Examples:
> bitshares-cli blockchain_get_blockcount
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "blockchain_get_blockcount", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
         )" } ;
    fc::variant rpc_server_impl::blockchain_get_blockcount(const fc::variants& params)
    {
      return fc::variant(_client->get_chain()->get_head_block_num());
    }


    static rpc_server::method_data wallet_open_file_metadata{"wallet_open_file", nullptr,
                                     /* description */ "Opens the wallet at the given path.",
                                     /* returns: */    "bool",
                                     /* params:          name                 type      required */
                                                       {{"wallet_name",   "path", true},
                                                        {"password",      "string", true} },
                                   /* prerequisites */ rpc_server::json_authenticated,
								   R"(
Wallets exist in the wallet data directory
								   )"};
    fc::variant rpc_server_impl::wallet_open_file(const fc::variants& params)
    {
      try
      {
         _client->get_wallet()->open_file( params[0].as_string(), params[1].as_string() );
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


    static rpc_server::method_data wallet_open_metadata{"wallet_open", nullptr,
                                     /* description */ "Opens the wallet of the given name",
                                     /* returns: */    "bool",
                                     /* params:          name           type      required */
                                                       {{"wallet_name", "string", true},
                                                        {"password",    "string", true} },
                                   /* prerequisites */ rpc_server::json_authenticated,
								   R"(
Wallets exist in the wallet data directory
								   )"};
    fc::variant rpc_server_impl::wallet_open(const fc::variants& params)
    {
      try
      {
        _client->get_wallet()->open( params[0].as_string(), params[1].as_string() );
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

    static rpc_server::method_data wallet_create_metadata{"wallet_create", nullptr,
                                     /* description */ "Opens the wallet of the given name",
                                     /* returns: */    "bool",
                                     /* params:          name           type      required */
                                                       {{"wallet_name", "string", true},
                                                        {"password",    "string", true} },
                                   /* prerequisites */ rpc_server::json_authenticated,
								   R"(
Wallets exist in the wallet data directory
								   )"};
    fc::variant rpc_server_impl::wallet_create(const fc::variants& params)
    { try {
        _client->get_wallet()->create( params[0].as_string(), params[1].as_string() );
        return fc::variant(true);
    } FC_RETHROW_EXCEPTIONS( warn, "" ) }



    static rpc_server::method_data wallet_get_name_metadata{"wallet_get_name", nullptr,
                                        /* description */ "Returns the wallet name passed to wallet_open",
                                        /* returns: */    "string",
                                        /* params:     */ {},
                                        /* prerequisites */ rpc_server::no_prerequisites,
									  R"(
									  )" };
    fc::variant rpc_server_impl::wallet_get_name(const fc::variants& params)
    {
       if( !_client->get_wallet()->is_open() )
          return fc::variant(nullptr);
       return fc::variant(_client->get_wallet()->get_name());
    }

    static rpc_server::method_data wallet_close_metadata{"wallet_close", nullptr,
                                      /* description */ "Closes the curent wallet if one is open.",
                                      /* returns: */    "bool",
                                      /* params:     */ {},
                                      /* prerequisites */ rpc_server::no_prerequisites,
									R"(
									)" };
    fc::variant rpc_server_impl::wallet_close(const fc::variants& params)
    {
       return fc::variant( _client->get_wallet()->close() );
    }

    static rpc_server::method_data wallet_lock_metadata{"wallet_lock", nullptr,
                                     /* description */ "Lock the private keys in wallet, disables spending commands until unlocked",
                                     /* returns: */    "void",
                                     /* params:     */ {},
                                   /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open};
    fc::variant rpc_server_impl::wallet_lock(const fc::variants& params)
    {
       try
       {
         _client->get_wallet()->lock();
         return fc::variant();
       }
       catch (...)
       {
         throw rpc_wallet_passphrase_incorrect_exception();
       }
    }

    static rpc_server::method_data wallet_unlock_metadata{"wallet_unlock", nullptr,
          /* description */ "Unlock the private keys in the wallet to enable spending operations",
          /* returns: */    "void",
          /* params:          name                   type      required */
                            {{"spending_pass",       "string", true},
                             {"timeout",             "int",    true} },
        /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
Stores the wallet decryption key in memory for 'timeout' seconds.
This is needed prior to performing transactions related to private keys such as sending bitcoins

Arguments:
1. "spending_pass" (string, required) The wallet spending passphrase
2. timeout (numeric, required) The time to keep the decryption key in seconds.

Note:
Issuing the wallet_unlock command while the wallet is already unlocked will set a new unlock
time that overrides the old one if the new time is longer than the old one, but you can immediately
lock the wallet with the wallet_lock command.

Examples:

unlock the wallet for 60 seconds
> bitshares-cli wallet_unlock "my pass phrase" 60

Lock the wallet again (before 60 seconds)
> bitshares-cli wallet_lock

As json rpc call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "wallet_unlock", "params": ["my pass phrase", 60] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )"};
    fc::variant rpc_server_impl::wallet_unlock(const fc::variants& params)
    {
       std::string passphrase = params[0].as_string();
       uint32_t timeout_sec = (uint32_t)params[1].as_uint64();
       try
       {
         _client->get_wallet()->unlock(passphrase, fc::seconds(timeout_sec));
         return fc::variant();
       }
       catch (...)
       {
         throw rpc_wallet_passphrase_incorrect_exception();
       }
    }

    static rpc_server::method_data wallet_create_receive_account_metadata{"wallet_create_receive_account", nullptr,
            /* description */ "Add new account for receiving payments",
            /* returns: */    "extended_address",
            /* params:          name       type       required */
                              {{"account_name", "string", true} },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
     )"};
    fc::variant rpc_server_impl::wallet_create_receive_account( const fc::variants& params )
    {
       auto receive_account_record = _client->get_wallet()->create_receive_account( params[0].as_string() );
       return fc::variant(extended_address(receive_account_record.extended_key));
    }

    static rpc_server::method_data wallet_create_sending_account_metadata{"wallet_create_sending_account", nullptr,
            /* description */ "Add new account for sending payments",
            /* returns: */    "null",
            /* params:          name       type       required */
                              {{"account_name", "string", true} ,
                               {"account_key", "extended_address", true} },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
     )"};
    fc::variant rpc_server_impl::wallet_create_sending_account( const fc::variants& params )
    {
       _client->get_wallet()->create_sending_account( params[0].as_string(), params[1].as<extended_address>() );
       return fc::variant();
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
    { try {
       bts::blockchain::address destination_address = params[0].as<bts::blockchain::address>();
       ilog( "destination: ${d}", ("d",destination_address) );
       auto amount = params[1].as<int64_t>();
       std::string comment;
       if (params.size() >= 3)
         comment = params[2].as_string();
       // TODO: we're currently ignoring optional parameter 4, [to-comment]
       FC_ASSERT( !"not implemented" );
       return fc::variant(); //_client->get_wallet()->send_to_address(asset(amount), destination_address, comment));
    } FC_RETHROW_EXCEPTIONS( warn, "" ) }

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

    static rpc_server::method_data wallet_transfer_metadata{"wallet_transfer", nullptr,
            /* description */ "Sends given amount to the given address, assumes shares in DAC",
            /* returns: */    "invoice_summary",
            /* params:          name          type       required */
                              { {"amount",                 "int64",  true  },
                                {"sending_account_name",   "string", true  },
                                {"invoice_memo",           "string", false },
                                {"from_account",           "string", false },
                                {"asset_id",               "int",    false }
                              },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
          )" };
    fc::variant rpc_server_impl::wallet_transfer(const fc::variants& params)
    {
       auto          amount     = params[0].as_int64();
       std::string   to_account = params[1].as_string();
       asset_id_type asset_id   = 0;
       std::string from_account = "*";
       std::string invoice_memo = "";
       if( params.size() == 3 )
       {
        auto        optional_params = params[0].get_object();
        if( optional_params.contains( "invoice_memo" ) )
           invoice_memo = optional_params["invoice_memo"].as_string();
        if( optional_params.contains( "from_account" ) )
           from_account = optional_params["from_account"].as_string();
        if( optional_params.contains( "asset_id" ) )
           asset_id = optional_params["asset_id"].as_int64();
       }
       auto summary = _client->get_wallet()->transfer( to_account, asset( amount, asset_id ), from_account, invoice_memo );
       return fc::variant(summary);
    }


    static rpc_server::method_data wallet_list_sending_accounts_metadata{"wallet_list_sending_accounts", nullptr,
            /* description */ "Lists all foreign addresses and their labels associated with this wallet",
            /* returns: */    "vector<string>",
            /* params:     */ { {"start", "int", false},
                                {"count", "int", false} },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_list_sending_accounts(const fc::variants& params)
    {  try {
      int32_t start = 0;
      uint32_t count = -1;
      if( params.size() > 0 ) start = params[0].as_int64();
      if( params.size() > 1 ) count = params[1].as_int64();

      auto accounts = _client->get_wallet()->list_sending_accounts(start,count);
      return fc::variant( accounts );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }


    static rpc_server::method_data wallet_list_receive_accounts_metadata{"wallet_list_receive_accounts", nullptr,
            /* description */ "Lists all foreign addresses and their labels associated with this wallet",
            /* returns: */    "vector<string>",
            /* params:     */ { {"start", "int", false},
                                {"count", "int", false} },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_list_receive_accounts(const fc::variants& params)
    {  try {
      int32_t start = 0;
      uint32_t count = -1;
      if( params.size() > 0 ) start = params[0].as_int64();
      if( params.size() > 1 ) count = params[1].as_int64();

      auto accounts = _client->get_wallet()->list_receive_accounts(start,count);
      return fc::variant( accounts );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data wallet_get_account_metadata{"wallet_get_account", nullptr,
            /* description */ "Lists all foreign addresses and their labels associated with this wallet",
            /* returns: */    "account_record",
            /* params:     */ { {"account_name", "string", true}
                                 },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_get_account(const fc::variants& params)
    {  try {
      auto account_record = _client->get_wallet()->get_account(params[0].as_string());
      fc::mutable_variant_object result;
      result["name"] = account_record.name;
      result["extended_address"] = extended_address( account_record.extended_key );
      return fc::variant( std::move(result) );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }


    static rpc_server::method_data wallet_get_balance_metadata{"wallet_get_balance", nullptr,
            /* description */ "Returns the wallet's current balance",
            /* returns: */    "asset",
            /* params:          name     type     required */
                              {{"account_name", "string",  false},
                               {"minconf",      "int",    false},
                               {"asset",        "int",    false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
TODO: HOW SHOULD THIS BEHAVE WITH ASSETS AND ACCOUNTS?
wallet_get_balance ( "account" min_confirms )

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
> bitshares-cli wallet_get_balance

The total amount in the server across all accounts, with at least 5 confirmations
> bitshares-cli wallet_get_balance "*" 6

The total amount in the default account with at least 1 confirmation
> bitshares-cli wallet_get_balance ""

The total amount in the account named tabby with at least 6 confirmations
> bitshares-cli wallet_get_balance "tabby" 6

As a json rpc call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "wallet_get_balance", "params": ["tabby", 6] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::wallet_get_balance(const fc::variants& params)
    {
      bts::blockchain::asset_id_type asset_id = 0;
      std::string account_name = "*";
      if (params.size() > 0 )
        account_name = params[0].as_string();
      if (params.size() == 3)
        asset_id = params[2].as<bts::blockchain::asset_id_type>();
      return fc::variant( _client->get_wallet()->get_balance( account_name, asset_id ) );
    }


    static rpc_server::method_data wallet_get_transaction_history_metadata{"wallet_get_transaction_history", nullptr,
            /* description */ "Retrieves all transactions into or out of this wallet.",
            /* returns: */    "std::vector<transaction_state>",
            /* params:          name     type     required */
                              {{"count", "unsigned",  false}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_get_transaction_history(const fc::variants& params)
    {
      unsigned count = 0; // TODO implement count
      if (params.size() == 1)
          count = params[0].as<unsigned>();

      return fc::variant( _client->get_wallet()->transactions() );
    }

    static rpc_server::method_data blockchain_get_name_record_metadata{"blockchain_get_name_record", nullptr,
            /* description */ "Retrieves the name record",
            /* returns: */    "name_record",
            /* params:          name              type               required */
                             {{"name",          "string",            true}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
     )" };
    fc::variant rpc_server_impl::blockchain_get_name_record(const fc::variants& params)
    {
      return fc::variant( _client->get_chain()->get_name_record(params[0].as_string()) );
    }
    static rpc_server::method_data wallet_reserve_name_metadata{"wallet_reserve_name", nullptr,
            /* description */ "Retrieves the name record",
            /* returns: */    "name_record",
            /* params:          name              type               required */
                             {{"name",          "string",            true},
                              {"data",          "variant",           true}},
            /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_reserve_name(const fc::variants& params)
    {
       return fc::variant( _client->reserve_name(params[0].as_string(), params[1]) );
    }
    static rpc_server::method_data wallet_register_delegate_metadata{"wallet_register_delegate", nullptr,
            /* description */ "Registeres a delegate to be voted upon by shareholders.",
            /* returns: */    "name_record",
            /* params:          name              type               required */
                             {{"name",          "string",            true},
                              {"data",          "variant",           true}},
            /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_register_delegate(const fc::variants& params)
    {
       return fc::variant( _client->register_delegate(params[0].as_string(), params[1]) );
    }

    static rpc_server::method_data blockchain_get_transaction_metadata{"blockchain_get_transaction", nullptr,
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
> bitshares-cli blockchain_get_transaction "1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "blockchain_get_transaction", "params": ["1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::blockchain_get_transaction(const fc::variants& params)
    {
      return fc::variant( _client->get_chain()->get_transaction( params[0].as<transaction_id_type>() )  );
    }

    static rpc_server::method_data blockchain_get_block_metadata{"blockchain_get_block", nullptr,
            /* description */ "Retrieves the block header for the given block hash",
            /* returns: */    "block_header",
            /* params:          name              type        required */
                              {{"block_hash",   "block_id_type", true}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
blockchain_get_block "hash" ( verbose )

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
> bitshares-cli blockchain_get_block "00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "blockchain_get_block", "params": ["00000000c937983704a73af28acdec37b049d214adbda81d7e2a3dd146f6ed09"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::blockchain_get_block(const fc::variants& params)
    { try {
      return fc::variant( _client->get_chain()->get_block( params[0].as<block_id_type>() )  );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data blockchain_get_block_by_number_metadata{"blockchain_get_block_by_number", nullptr,
                                   /* description */ "Retrieves the block header for the given block number",
                                   /* returns: */    "block_header",
                                   /* params:          name              type        required */
                                                     {{"block_number",   "int32", true}},
                                 /* prerequisites */ rpc_server::json_authenticated};
    fc::variant rpc_server_impl::blockchain_get_block_by_number(const fc::variants& params)
    { try {
      return fc::variant( _client->get_chain()->get_block( params[0].as<uint32_t>() ) );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data validate_address_metadata{"validate_address", nullptr,
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
> bitshares-cli validate_address "1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "validate_address", "params": ["1PSSGeFHDnKNxiEyFrD1wcEaHr9hrQDDWc"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::validate_address(const fc::variants& params)
    {
      try {
        return fc::variant(params[0].as_string());
      }
      catch ( const fc::exception& )
      {
        return fc::variant(false);
      }
    }

    static rpc_server::method_data wallet_rescan_blockchain_metadata{"wallet_rescan_blockchain", nullptr,
            /* description */ "Rescan the block chain from the given block",
            /* returns: */    "bool",
            /* params:          name              type    required */
                              {{"starting_block", "bool", false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_rescan_blockchain(const fc::variants& params)
    {
      uint32_t block_num = 0;
      if (params.size() == 1)
        block_num = (uint32_t)params[0].as_int64();
      _client->get_wallet()->scan_chain( block_num);
      return fc::variant(true);
    }

    static rpc_server::method_data wallet_rescan_blockchain_state_metadata{"wallet_rescan_blockchain_state", nullptr,
            /* description */ "Rescans the genesis block and state (not the transactions)",
            /* returns: */    "bool",
            /* params:          name              type    required */
                              {},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_rescan_blockchain_state(const fc::variants& params)
    {
      _client->get_wallet()->scan_state();
      return fc::variant(true);
    }

    static rpc_server::method_data wallet_import_bitcoin_metadata{"wallet_import_bitcoin", nullptr,
            /* description */ "Import a BTC/PTS wallet",
            /* returns: */    "bool",
            /* params:          name               type       required */
                              {{"filename", "string",  true}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_import_bitcoin(const fc::variants& params)
    {
      auto wallet_dat      = params[0].as<fc::path>();
      auto wallet_password = params[1].as<std::string>();
      _client->get_wallet()->import_bitcoin_wallet( wallet_dat, wallet_password );
      return fc::variant(true);
    }


    // TODO: get account argument
    static rpc_server::method_data wallet_import_private_key_metadata{"wallet_import_private_key", nullptr,
            /* description */ "Import a BTC/PTS private key in wallet import format (WIF)",
            /* returns: */    "bool",
            /* params:          name           type            required */
                              {{"key",             "private_key",  true},
                                {"account_name",   "string",       false},
                                {"wallet_rescan_blockchain",         "bool",         false}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
          R"(
wallet_import_private_key "bitcoinprivkey" ( "account" wallet_rescan_blockchain "address_label")

Adds a private key (as returned by dumpprivkey) to your wallet.

Arguments:
1. "bitcoinprivkey" (string, required) The private key (see dumpprivkey)
2. "account" (string, optional) the name of the account to put this key in
3. wallet_rescan_blockchain (boolean, optional, default=true) Rescan the wallet for transactions
4. "address_label" (string,optional) assigns a label to the address being imported

Examples:

Dump a private key from Bitcoin wallet
> bitshares-cli dumpprivkey "myaddress"

Import the private key to BitShares wallet
> bitshares-cli wallet_import_private_key "mykey"

Import using an account name
> bitshares-cli wallet_import_private_key "mykey" "testing" false "address_label"

As a json rpc call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "wallet_import_private_key", "params": ["mykey", "testing", false] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
     )" };
    fc::variant rpc_server_impl::wallet_import_private_key(const fc::variants& params)
    {
      auto wif   =  params[0].as_string();
      auto label =  params[1].as_string();
      bool wallet_rescan_blockchain = false;
      if (params.size() == 3 && params[2].as_bool())
        wallet_rescan_blockchain = true;

      _client->get_wallet()->import_wif_private_key(wif, label);

      if (wallet_rescan_blockchain)
          _client->get_wallet()->scan_chain(0);

      return fc::variant(true);
    }

    static rpc_server::method_data blockchain_get_names_metadata{"blockchain_get_names", nullptr,
            /* description */ "Returns the list of reserved names sorted alphabetically",
            /* returns: */    "vector<name_record>",
            /* params:     */ { {"first", "string", false},
                                {"count", "int", false} },
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
blockchain_get_names (first, count)

Returns up to count reserved names that follow first alphabetically.
             )" };
    fc::variant rpc_server_impl::blockchain_get_names(const fc::variants& params)
    {
      std::string first;
      uint32_t count = uint32_t(-1);
      if( params.size() > 0 )
         first = params[0].as_string();
      if( params.size() > 1 )
         count = params[1].as<uint32_t>();
      return fc::variant(_client->get_chain()->get_names( first, count ) );
    }

    static rpc_server::method_data blockchain_get_delegates_metadata{"blockchain_get_delegates", nullptr,
            /* description */ "Returns the list of delegates sorted by vote",
            /* returns: */    "vector<name_record>",
            /* params:     */ { {"first", "int", false},
                                {"count", "int", false} },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
blockchain_get_delegates (start, count)

Returns information about the delegates sorted by their net votes starting from position start and returning up to count items.

Arguments:
   first - the index of the first delegate to be returned
   count - the maximum number of delegates to be returned

             )" };

    fc::variant rpc_server_impl::blockchain_get_delegates(const fc::variants& params)
    {
      uint32_t first = 0;
      uint32_t count = 0;
      if( params.size() > 0 ) 
        first = params[0].as<uint32_t>();
      if( params.size() > 1 ) 
        count = params[1].as<uint32_t>();
      return fc::variant(_client->get_chain()->get_delegates_by_vote(first, count) );
    }

    static rpc_server::method_data network_get_connection_count_metadata{"network_get_connection_count", nullptr,
            /* description */ "Returns the number of connections to other nodes.",
            /* returns: */    "bool",
            /* params:     */ {},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
Result:
n (numeric) The connection count

Examples:
> bitshares-cli network_get_connection_count
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "network_get_connection_count", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/

)" };
    fc::variant rpc_server_impl::network_get_connection_count(const fc::variants&)
    {
      return fc::variant(_client->get_connection_count());
    }

    static rpc_server::method_data network_get_peer_info_metadata{"network_get_peer_info", nullptr,
            /* description */ "Returns data about each connected node.",
            /* returns: */    "vector<jsonobject>",
            /* params:     */ {},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
network_get_peer_info

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
> bitcoin-cli network_get_peer_info
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "network_get_peer_info", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
)" };
    fc::variant rpc_server_impl::network_get_peer_info(const fc::variants&)
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

    static rpc_server::method_data network_add_node_metadata{"network_add_node", nullptr,
            /* description */ "Attempts add or remove <node> from the peer list or try a connection to <node> once",
            /* returns: */    "null",
            /* params:          name            type            required */
                              {{"node",         "string",       true},
                               {"command",      "string",       true}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
network_add_node "node" "add|remove|onetry"

Attempts add or remove a node from the network_add_node list.
Or try a connection to a node once.

Arguments:
1. "node" (string, required) The node (see network_get_peer_info for nodes)
2. "command" (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the node once

Examples:
> bitcoin-cli network_add_node "192.168.0.6:8333" "onetry"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "network_add_node", "params": ["192.168.0.6:8333", "onetry"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
)" };
    fc::variant rpc_server_impl::network_add_node(const fc::variants& params)
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

    static rpc_server::method_data _get_transaction_propagation_data_metadata{"_get_transaction_propagation_data", nullptr,
            /* description */ "Returns the time the transaction was first seen by this client",
            /* returns: */    "bts::net::message_propagation_data",
            /* params:          name              type               required */
                              {{"transaction_id", "transaction_id",  true}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
_get_transaction_propagation_data <transaction_id>

Returns the time the transaction was first seen by this client.

This interrogates the p2p node's message cache to find out when it first saw this transaction.
The data in the message cache is only kept for a few blocks, so you can only use this to ask
about recent transactions. This is intended to be used to track message propagation delays
in our test network.
)" };
    fc::variant rpc_server_impl::_get_transaction_propagation_data(const fc::variants& params)
    {
      return fc::variant(_client->get_transaction_propagation_data(params[0].as<transaction_id_type>()));
    }
    static rpc_server::method_data _get_block_propagation_data_metadata{"_get_block_propagation_data", nullptr,
            /* description */ "Returns the time the block was first seen by this client",
            /* returns: */    "bts::net::message_propagation_data",
            /* params:          name              type        required */
                              {{"block_hash",   "block_id_type", true}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
_get_block_propagation_data <block_hash>

Returns the time the block was first seen by this client.

This interrogates the p2p node's message cache to find out when it first saw this transaction.
The data in the message cache is only kept for a few blocks, so you can only use this to ask
about recent transactions. This is intended to be used to track message propagation delays
in our test network.
)" };
    fc::variant rpc_server_impl::_get_block_propagation_data(const fc::variants& params)
    {
      return fc::variant(_client->get_block_propagation_data(params[0].as<block_id_type>()));
    }

    static rpc_server::method_data _list_json_commands_metadata{"_list_json_commands", nullptr,
        /* description */ "Lists commands.",
        /* returns: */    "vector<string>",
        /* params:     */ {},
      /* prerequisites */ rpc_server::no_prerequisites,
      R"(
Result:
"commands" (array of strings) The names of all supported json commands
       )"};
    fc::variant rpc_server_impl::_list_json_commands(const fc::variants& params)
    {
      std::vector<std::string> commands;
      for (const method_map_type::value_type& method_data_pair : _method_map)
        commands.push_back(method_data_pair.first);
      return fc::variant(commands);
    }

  } // detail

  bool rpc_server::config::is_valid() const
  {
//    if (rpc_user.empty())
//      return false;
//    if (rpc_password.empty())
//      return false;
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

