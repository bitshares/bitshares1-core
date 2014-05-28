#include <bts/rpc/rpc_server.hpp>
#include <bts/utilities/git_revision.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fc/interprocess/file_mapping.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/server.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>
#include <fc/git_revision.hpp>

#include <iomanip>
#include <limits>
#include <sstream>

namespace bts { namespace rpc {

#define RPC_METHOD_LIST\
             (help)\
             (get_info)\
             (stop)\
             (validate_address)\
             (blockchain_get_blockhash)\
             (blockchain_get_blockcount)\
             (blockchain_get_transaction)\
             (blockchain_get_block)\
             (blockchain_get_block_by_number)\
             (blockchain_get_name_record)\
             (blockchain_get_name_record_by_id)\
             (blockchain_get_asset_record)\
             (blockchain_get_asset_record_by_id)\
             (blockchain_get_assets)\
             (blockchain_get_delegates)\
             (blockchain_get_names)\
             (network_get_connection_count)\
             (network_get_peer_info)\
             (network_add_node)\
             (wallet_open_file)\
             (wallet_create)\
             (wallet_open)\
             (wallet_get_name)\
             (wallet_close)\
             (wallet_export_to_json)\
             (wallet_create_from_json)\
             (wallet_lock)\
             (wallet_unlock)\
             (wallet_change_passphrase)\
             (wallet_create_account) \
             (wallet_add_contact_account) \
             (wallet_import_private_key)\
             (wallet_import_bitcoin)\
             (wallet_list_contact_accounts)\
             (wallet_list_receive_accounts)\
             (wallet_list_reserved_names)\
             (wallet_get_account)\
             (wallet_rename_account)\
             (wallet_asset_create)\
             (wallet_asset_issue)\
             (wallet_get_balance)\
             (wallet_get_transaction_history)\
             (wallet_get_transaction_history_summary)\
             (wallet_rescan_blockchain)\
             (wallet_rescan_blockchain_state)\
             (wallet_register_account)\
             (wallet_submit_proposal)\
             (wallet_vote_proposal)\
             (wallet_set_delegate_trust_status)\
             (wallet_get_delegate_trust_status)\
             (wallet_list_delegate_trust_status)\
             (network_get_block_propagation_data)\
             (network_get_transaction_propagation_data)\
             (_list_json_commands)\
             (wallet_get_pretty_transaction)\
             (network_broadcast_transaction)\
             (network_set_advanced_node_parameters)

  namespace detail
  {
    class rpc_server_impl
    {
       public:
         rpc_server::config                _config;
         client_ptr                        _client;
         fc::http::server                  _httpd;
         fc::tcp_server                    _tcp_serv;
         fc::future<void>                  _accept_loop_complete;
         rpc_server*                       _self;
         fc::shared_ptr<fc::promise<void>> _on_quit_promise;

         typedef std::map<std::string, rpc_server::method_data> method_map_type;
         method_map_type _method_map;
        
         /** the map of alias and method name */
         std::map<std::string, std::string>     _alias_map;

         /** the set of connections that have successfully logged in */
         std::unordered_set<fc::rpc::json_connection*> _authenticated_connection_set;

         std::string make_short_description(const rpc_server::method_data& method_data)
         {
           std::string help_string;
           std::stringstream sstream;
           //format into columns
           sstream << std::setw(100) << std::left;
           help_string = method_data.name + " ";
           for (const rpc_server::parameter_data& parameter : method_data.parameters)
           {
             if (parameter.classification == rpc_server::required_positional)
               help_string += std::string("<") + parameter.name + std::string("> ");
             else if (parameter.classification == rpc_server::optional_named)
               help_string += std::string("{") + parameter.name + std::string("} ");
             else if (parameter.classification == rpc_server::required_positional_hidden)
               continue;
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
                   if(method_name.find("wallet") != std::string::npos || method_name.find("priv") != std::string::npos)
                       params_log = "***";
                   fc_ilog( fc::logger::get("rpc"), "Processing ${path} ${method} (${params})", ("path",r.path)("method",method_name)("params",params_log));

                   auto call_itr = _alias_map.find( method_name );
                   if( call_itr != _alias_map.end() )
                   {
                      fc::mutable_variant_object  result;
                      result["id"]     =  rpc_call["id"];
                      try
                      {
                         result["result"] = dispatch_authenticated_method(_method_map[call_itr->second], params);
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
                      //ilog( "${e}", ("e",result) );
                      auto reply = fc::json::to_string( result );
                      s.set_length( reply.size() );
                      s.write( reply.c_str(), reply.size() );
                      auto reply_log = reply.size() > 253 ? reply.substr(0,253) + ".." :  reply;
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
              auto bind_method = boost::bind(&rpc_server_impl::dispatch_method_from_json_connection,
                                                        this, capture_con, method.second, _1);
              con->add_method(method.first, bind_method);
              for ( auto alias : method.second.aliases )
              {
                  con->add_method(alias, bind_method);
              }
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
                                                  const fc::variants& arguments_from_caller)
        {
          //ilog("method: ${method_name}, arguments: ${params}", ("method_name", method_data.name)("params",arguments_from_caller));
          if (method_data.prerequisites & rpc_server::wallet_open)
            check_wallet_is_open();
          if (method_data.prerequisites & rpc_server::wallet_unlocked)
            check_wallet_unlocked();
          if (method_data.prerequisites & rpc_server::connected_to_network)
            check_connected_to_network();

          fc::variants modified_positional_arguments;
          fc::mutable_variant_object modified_named_arguments;

          bool has_named_parameters = false;
          unsigned next_argument_index = 0;
          for (const rpc_server::parameter_data& parameter : method_data.parameters)
          {
            if (parameter.classification == rpc_server::required_positional
                || parameter.classification == rpc_server::required_positional_hidden)
            {
              if (arguments_from_caller.size() < next_argument_index + 1)
                FC_THROW_EXCEPTION(exception, "missing required parameter ${parameter_name}", ("parameter_name", parameter.name));
              modified_positional_arguments.push_back(arguments_from_caller[next_argument_index++]);
            }
            else if (parameter.classification == rpc_server::optional_positional)
            {
              if( arguments_from_caller.size() > next_argument_index )
                // the caller provided this optional argument
                modified_positional_arguments.push_back(arguments_from_caller[next_argument_index++]);
              else if( parameter.default_value.valid() )
                // we have a default value for this paramter, use it
                modified_positional_arguments.push_back(*parameter.default_value);
              else
                // missing optional parameter with no default value, stop processing
                break;
            }
            else if (parameter.classification == rpc_server::optional_named)
            {
              has_named_parameters = true;
              if (arguments_from_caller.size() > next_argument_index)
              {
                // user provided a map of named arguments.  If the user gave a value for this argument,
                // take it, else use our default value
                fc::variant_object named_parameters_from_caller = arguments_from_caller[next_argument_index].get_object();
                if (named_parameters_from_caller.contains(parameter.name.c_str()))
                  modified_named_arguments[parameter.name.c_str()] = named_parameters_from_caller[parameter.name.c_str()];
                else if( parameter.default_value.valid() )
                  modified_named_arguments[parameter.name.c_str()] = *parameter.default_value;
              }
              else if( parameter.default_value.valid() )
              {
                // caller didn't provide any map of named parameters, just use our default values
                modified_named_arguments[parameter.name.c_str()] = *parameter.default_value;
              }
            }
          }

          if (has_named_parameters)
            modified_positional_arguments.push_back(modified_named_arguments);

          //ilog("After processing: method: ${method_name}, arguments: ${params}", ("method_name", method_data.name)("params",modified_positional_arguments));

          return method_data.method(modified_positional_arguments);
        }

        // This method invokes the function directly, called by the CLI intepreter.
        fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
        {
          // ilog( "method: ${method} arguments: ${params}", ("method",method_name)("params",arguments) );
          auto iter = _alias_map.find(method_name);
          if (iter == _alias_map.end())
            FC_THROW_EXCEPTION(exception, "Invalid command ${command}", ("command", method_name));
          return dispatch_authenticated_method(_method_map[iter->second], arguments);
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
        /* description */ "Lists commands or detailed help for specified command",
        /* returns: */    "bool",
        /* params:          name       type      classification                   default value */
                         {{ "command", "string", rpc_server::optional_positional, fc::ovariant() } },
      /* prerequisites */ rpc_server::no_prerequisites,
      R"(
Arguments:
1. "command" (string, optional) The command to get help on

Result:
"text" (string) The help text
       )",
        /*aliases*/ { "h" }};
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
          if (method_data.aliases.size() > 0)
          {
            std::stringstream ss;
            ss << "\naliases: ";
            for (size_t i = 0; i < method_data.aliases.size(); ++i)
            {
              if (i != 0)
                ss << ", ";
              ss << method_data.aliases[i];
            }
            help_string += ss.str();
          }
        }
        else
        {
          // no exact matches for the command they requested.
          // If they give us a prefix, give them the list of commands that start
          // with that prefix (i.e. "help wallet" will return wallet_open, wallet_close, &c)
          std::vector<std::string> match_commands;
          for (itr = _method_map.lower_bound(command);
               itr != _method_map.end() && itr->first.compare(0, command.size(), command) == 0;
               ++itr)
            match_commands.push_back(itr->first);
          // If they give us a alias(or its prefix), give them the list of real command names, eliminating duplication
          for (auto alias_itr = _alias_map.lower_bound(command);
                  alias_itr != _alias_map.end() && alias_itr->first.compare(0, command.size(), command) == 0;
                  ++alias_itr)
          {
            if (std::find(match_commands.begin(), match_commands.end(), alias_itr->second) == match_commands.end())
            {
              match_commands.push_back(alias_itr->second);
            }
          }
          for (auto c : match_commands)
            help_string += make_short_description(_method_map[c]);
          if (help_string.empty())
            throw rpc_misc_error_exception(FC_LOG_MESSAGE( error, "No help available for command \"${command}\"", ("command", command)));
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
                                   /* prerequisites */ 0,
                                   R"(
                                       )",
                                    /* aliases */ { "getinfo" } } ;
    fc::variant rpc_server_impl::get_info(const fc::variants& /*params*/)
    {
       fc::mutable_variant_object info;
       auto share_record = _client->get_chain()->get_asset_record( BTS_ADDRESS_PREFIX );
       auto current_share_supply = share_record.valid() ? share_record->current_share_supply : 0;
       auto bips_per_share = current_share_supply > 0 ? double( BTS_BLOCKCHAIN_BIP ) / current_share_supply : 0;
       auto advanced_params = _client->network_get_advanced_node_parameters();
       auto wallet_balance_shares = _client->get_wallet()->is_open() ? _client->get_wallet()->get_balance().amount : 0;

       info["blockchain_asset_reg_fee"]             = BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE;
       info["blockchain_asset_shares_max"]          = BTS_BLOCKCHAIN_MAX_SHARES;

       info["blockchain_bips_per_share"]            = bips_per_share;

       info["blockchain_block_fee_min"]             = double( BTS_BLOCKCHAIN_MIN_FEE ) / 1000;
       info["blockchain_block_interval"]            = BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
       info["blockchain_block_num"]                 = _client->get_chain()->get_head_block_num();
       info["blockchain_block_size_max"]            = BTS_BLOCKCHAIN_MAX_BLOCK_SIZE;
       info["blockchain_block_size_target"]         = BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE;

       info["blockchain_delegate_fire_votes_min"]   = BTS_BLOCKCHAIN_FIRE_VOTES;
       info["blockchain_delegate_num"]              = BTS_BLOCKCHAIN_NUM_DELEGATES;
       info["blockchain_delegate_reg_fee"]          = BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE;
       info["blockchain_delegate_reward_min"]       = BTS_BLOCKCHAIN_MIN_REWARD;

       info["blockchain_id"]                        = _client->get_chain()->chain_id();

       info["blockchain_name_size_max"]             = BTS_BLOCKCHAIN_MAX_NAME_SIZE;
       info["blockchain_name_data_size_max"]        = BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE;

       info["blockchain_random_seed"]               = _client->get_chain()->get_current_random_seed();

       info["blockchain_shares"]                    = current_share_supply;

       info["blockchain_size_max"]                  = BTS_BLOCKCHAIN_MAX_SIZE;

       info["blockchain_symbol"]                    = BTS_ADDRESS_PREFIX;

       info["blockchain_version"]                   = BTS_BLOCKCHAIN_VERSION;

       info["client_httpd_port"]                    = _config.is_valid() ? _config.httpd_endpoint.port() : 0;

       info["client_rpc_port"]                      = _config.is_valid() ? _config.rpc_endpoint.port() : 0;

       info["network_num_connections"]              = _client->network_get_connection_count();
       info["network_num_connections_max"]          = advanced_params["maximum_number_of_connections"];

       info["network_protocol_version"]             = BTS_NET_PROTOCOL_VERSION;

       info["wallet_balance"]                       = wallet_balance_shares;
       info["wallet_balance_bips"]                  = wallet_balance_shares * bips_per_share;

       info["wallet_open"]                          = _client->get_wallet()->is_open();

       info["wallet_unlocked_until"]                = _client->get_wallet()->is_open() && _client->get_wallet()->is_unlocked()
                                                    ? boost::posix_time::to_iso_string( boost::posix_time::from_time_t( _client->get_wallet()->unlocked_until().sec_since_epoch() ) )
                                                    : "";

       info["wallet_version"]                       = BTS_WALLET_VERSION;

       info["_bitshares_toolkit_revision"]          = bts::utilities::git_revision_sha;
       info["_fc_revision"]                         = fc::git_revision_sha;
       info["_network_node_id"]                     = _client->get_node_id();

       return fc::variant( std::move(info) );
    }

    static rpc_server::method_data blockchain_get_blockhash_metadata{"blockchain_get_blockhash", nullptr,
                                     /* description */ "Returns hash of block in best-block-chain at index provided",
                                     /* returns: */    "block_id_type",
                                     /* params:          name         type   classification                   default value */
                                                       {{"block_number", "int", rpc_server::required_positional, fc::ovariant()} },
                                      /* prerequisites */ rpc_server::no_prerequisites,
                                      R"(
    )",
        /*aliases*/ { "bitcoin_getblockhash", "getblockhash" }};
    fc::variant rpc_server_impl::blockchain_get_blockhash(const fc::variants& params)
    {
      return fc::variant(_client->blockchain_get_blockhash( (uint32_t)params[0].as_int64() ) );
    }


    static rpc_server::method_data blockchain_get_blockcount_metadata{"blockchain_get_blockcount", nullptr,
                                     /* description */ "Returns the number of blocks in the longest block chain",
                                     /* returns: */    "int",
                                     /* params:  */    { },
                                   /* prerequisites */ rpc_server::no_prerequisites,
         R"(
         )",
         /*aliases*/ { "bitcoin_getblockcount", "getblockcount" }} ;
    fc::variant rpc_server_impl::blockchain_get_blockcount(const fc::variants& params)
    {
      return fc::variant(_client->blockchain_get_blockcount());
    }


    static rpc_server::method_data wallet_open_file_metadata{"wallet_open_file", nullptr,
     /* description */ "Opens the wallet at the given path",
     /* returns: */    "void",
     /* params:          name             type      classification                          default value */
                       {{"wallet_file",   "path",   rpc_server::required_positional,        fc::ovariant()},
                        {"passphrase",    "string", rpc_server::required_positional_hidden, fc::ovariant()} },
   /* prerequisites */ rpc_server::json_authenticated,
   R"(
Wallets exist in the wallet data directory
   )"};
    fc::variant rpc_server_impl::wallet_open_file(const fc::variants& params)
    {
        try
        {
            _client->wallet_open_file( params[0].as_string() );
        }
        catch( const fc::exception& e ) // TODO: see wallet_unlock()
        {
            wlog( "${e}", ("e",e.to_detail_string() ) );
            throw rpc_wallet_passphrase_incorrect_exception();
        }
        return fc::variant( true );
    }

    static rpc_server::method_data wallet_open_metadata{"wallet_open", nullptr,
     /* description */ "Opens the wallet of the given name",
     /* returns: */    "void",
     /* params:          name             type      classification                          default value */
                       {
                          {"wallet_name",   "string", rpc_server::required_positional,        fc::ovariant()}
                       },
   /* prerequisites */ rpc_server::json_authenticated,
   R"(
Wallets exist in the wallet data directory
   )",
    /* aliases */ { "open" } 
    };
    fc::variant rpc_server_impl::wallet_open(const fc::variants& params)
    {
        try
        {
            _client->wallet_open( params[0].as_string() );
        }
        catch( const fc::exception& e ) // TODO: see wallet_unlock()
        {
            wlog( "${e}", ("e",e.to_detail_string() ) );
            throw rpc_wallet_passphrase_incorrect_exception();
        }
        return fc::variant();
    }

    static rpc_server::method_data wallet_create_metadata{"wallet_create", nullptr,
     /* description */ "Opens the wallet of the given name",
     /* returns: */    "void",
     /* params:          name             type      classification                          default value */
                       {{"wallet_name",   "string", rpc_server::required_positional,        fc::ovariant()},
                        {"passphrase",    "string", rpc_server::required_positional_hidden, fc::ovariant()}},
   /* prerequisites */ rpc_server::json_authenticated,
   R"(
Wallets exist in the wallet data directory
   )"};
    fc::variant rpc_server_impl::wallet_create(const fc::variants& params)
    { try {
        ilog( "args: ${args}", ("args",params));
        _client->wallet_create( params[0].as_string(), params[1].as_string() );
        return fc::variant();
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
       return fc::variant(_client->wallet_get_name());
    }

    static rpc_server::method_data wallet_close_metadata{"wallet_close", nullptr,
      /* description */ "Closes the curent wallet if one is open",
      /* returns: */    "void",
      /* params:     */ {},
      /* prerequisites */ rpc_server::no_prerequisites,
    R"(
    )",
      /* aliases */ { "close" } };
    fc::variant rpc_server_impl::wallet_close(const fc::variants& params)
    {
       _client->wallet_close();
       return fc::variant();
    }

    static rpc_server::method_data wallet_export_to_json_metadata{"wallet_export_to_json", nullptr,
      /* description */ "Exports the current wallet to a JSON file",
      /* returns: */    "void",
      /* params:         name           type     classification                          default_value */
                      {{"filename",    "path",   rpc_server::required_positional,        fc::ovariant()}},
      /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
    )" };
    fc::variant rpc_server_impl::wallet_export_to_json(const fc::variants& params)
    {
       auto filename = params[0].as<fc::path>();
       _client->wallet_export_to_json( filename );
       return fc::variant();
    }

    static rpc_server::method_data wallet_create_from_json_metadata{"wallet_create_from_json", nullptr,
      /* description */ "Creates a new wallet from an exported JSON file",
      /* returns: */    "void",
      /* params:         name           type     classification                          default_value */
                      {{"filename",    "path",   rpc_server::required_positional,        fc::ovariant()},
                       {"wallet_name", "string", rpc_server::required_positional,        fc::ovariant()}},
      /* prerequisites */ rpc_server::json_authenticated,
    R"(
    )" };
    fc::variant rpc_server_impl::wallet_create_from_json(const fc::variants& params)
    {
        auto filename = params[0].as<fc::path>();
        auto wallet_name = params[1].as_string();
        try
        {
            _client->wallet_create_from_json( filename, wallet_name );
        }
        catch( const fc::exception& e ) // TODO: see wallet_unlock()
        {
            wlog( "${e}", ("e",e.to_detail_string() ) );
            throw rpc_wallet_passphrase_incorrect_exception();
        }
        return fc::variant();
    }

    static rpc_server::method_data wallet_lock_metadata{"wallet_lock", nullptr,
         /* description */ "Lock the private keys in wallet, disables spending commands until unlocked",
         /* returns: */    "void",
         /* params:     */ {},
       /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
       R"(
        )",
        /*aliases*/ { "bitcoin_walletlock", "walletlock", "lock"  }};
    fc::variant rpc_server_impl::wallet_lock(const fc::variants& params)
    {
       _client->wallet_lock();
       return fc::variant();
    }

    static rpc_server::method_data wallet_unlock_metadata{"wallet_unlock", nullptr,
          /* description */ "Unlock the private keys in the wallet to enable spending operations",
          /* returns: */    "null",
          /* params:          name             type        classification                          default value */
                            {
                             {"timeout",       "int",      rpc_server::required_positional,        fc::ovariant()},
                             {"passphrase",    "string",   rpc_server::required_positional_hidden, fc::ovariant()} 
                            },
        /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
     )",
        /** aliases */ { "unlock" }
    };
    fc::variant rpc_server_impl::wallet_unlock(const fc::variants& params)
    {
       ilog( "${args}", ("args",params) );
        auto timeout_sec = params[0].as_uint64();
        std::string passphrase = params[1].as_string();
        try
        {
            _client->wallet_unlock(fc::seconds(timeout_sec), passphrase);
        }
        catch( const fc::exception& e )
        {
            /*
             * TODO: this is an invalid conversion to rpc_wallet_passphrase exception...
             * if the problem is 'file not found' or 'invalid user' or 'permission denined'
             * or some other filesystem error then it should be properly reported.
             */
            wlog( "${e}", ("e",e.to_detail_string() ) );
            throw rpc_wallet_passphrase_incorrect_exception();
        }
        return fc::variant();
    }

    static rpc_server::method_data wallet_change_passphrase_metadata{"wallet_change_passphrase", nullptr,
          /* description */ "Change the password of the current wallet",
          /* returns: */    "void",
          /* params:          name             type        classification                          default value */
                            { {"passphrase",    "string",   rpc_server::required_positional_hidden, fc::ovariant()} },
        /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
    R"(
Changes the wallet password.
This will change the wallet's spending passphrase, please make sure you remember it.

Arguments:
1. "passphrase" (string, required) The wallet new spending passphrase

Note:
Wallets exist in the wallet data directory.
     )"};
    fc::variant rpc_server_impl::wallet_change_passphrase(const fc::variants& params)
    {
       std::string passphrase = params[0].as_string();
       try
       {
         _client->wallet_change_passphrase( passphrase );
         return fc::variant();
       } FC_RETHROW_EXCEPTIONS( warn, "" )
    }

    static rpc_server::method_data wallet_create_account_metadata{"wallet_create_account", nullptr,
            /* description */ "Add new account for receiving payments",
            /* returns: */    "extended_address",
            /* params:          name             type      classification                   default value */
                              {{"account_name", "string",  rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
     )"};
    fc::variant rpc_server_impl::wallet_create_account( const fc::variants& params )
    {
       auto receive_address = _client->wallet_create_account( params[0].as_string() );
       return fc::variant(receive_address);
    }

    static rpc_server::method_data wallet_add_contact_account_metadata{"wallet_add_contact_account", nullptr,
            /* description */ "Add new account for sending payments",
            /* returns: */    "null",
            /* params:          name             type                classification                   default value */
                              {{"account_name",  "string",           rpc_server::required_positional, fc::ovariant()},
                               {"account_key",   "extended_address", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
    R"(
     )"};
    fc::variant rpc_server_impl::wallet_add_contact_account( const fc::variants& params )
    {
       _client->wallet_add_contact_account( params[0].as_string(), params[1].as<public_key_type>() );
       return fc::variant();
    }

    static rpc_server::method_data wallet_get_pretty_transaction_metadata{"wallet_get_pretty_transaction", nullptr,
            /* description */ "Return a pretty json representation of a transaction",
            /* returns: */    "pretty_transaction",
            /* params:          name                  type                   classification                   default_value */
                              {{"signed_transaction", "signed_transaction",  rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_get_pretty_transaction(const fc::variants& params)
    {
      bts::blockchain::signed_transaction transaction = params[0].as<bts::blockchain::signed_transaction>();
      auto rec = wallet_transaction_record( transaction );
      auto pretty = _client->get_wallet()->to_pretty_trx( rec );
      return fc::variant(pretty);
    }

    static rpc_server::method_data network_broadcast_transaction_metadata{"network_broadcast_transaction", nullptr,
            /* description */ "Broadcast a previously-created signed transaction to the network",
            /* returns: */    "transaction_id",
            /* params:          name                  type                   classification                   default_value */
                              {{"signed_transaction", "signed_transaction",  rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::connected_to_network,
          R"(
     )" };
    fc::variant rpc_server_impl::network_broadcast_transaction(const fc::variants& params)
    {
      bts::blockchain::signed_transaction transaction = params[0].as<bts::blockchain::signed_transaction>();
      _client->broadcast_transaction(transaction);
      return fc::variant(transaction.id());
    }

#if 0 
    static rpc_server::method_data wallet_transfer_metadata{"wallet_transfer", nullptr,
            /* description */ "Sends given amount to the given address, assumes shares in DAC",
            /* returns: */    "invoice_summary",
            /* params:          name                    type        classification                   default value */
                              {{"amount",               "int64",    rpc_server::required_positional, fc::ovariant()},
                               {"to_account_name",      "string",   rpc_server::required_positional, fc::ovariant()},
                               {"asset_symbol",         "string",   rpc_server::optional_named,      fc::variant(BTS_ADDRESS_PREFIX)},
                               {"from_account",         "string",   rpc_server::optional_named,      fc::variant("*")},
                               {"invoice_memo",         "string",   rpc_server::optional_named,      fc::variant("")}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
          )" };
    fc::variant rpc_server_impl::wallet_transfer(const fc::variants& params)
    {
       auto          amount     = params[0].as_int64();
       std::string   to_account_name = params[1].as_string();
       fc::variant_object named_params = params[2].get_object();
       std::string   asset_symbol = named_params["asset_symbol"].as_string();
       std::string   from_account_name = named_params["from_account"].as_string();
       std::string   invoice_memo = named_params["invoice_memo"].as_string();
       bts::wallet::invoice_summary summary = 
         _client->wallet_transfer(amount, to_account_name, asset_symbol, from_account_name, invoice_memo);
       return fc::variant(summary);
    }
#endif

    static rpc_server::method_data wallet_asset_create_metadata{"wallet_asset_create", nullptr,
            /* description */ "Creates a new user issued asset",
            /* returns: */    "invoice_summary",
            /* params:          name                    type        classification                   default value */
                              {{"symbol",               "string",   rpc_server::required_positional, fc::ovariant()},
                               {"asset_name",           "string",   rpc_server::required_positional, fc::ovariant()},
                               {"issuer_name",          "string",   rpc_server::required_positional, fc::ovariant()},
                               {"description",          "string",   rpc_server::optional_named,      fc::variant("")},
                               {"data",                 "json",     rpc_server::optional_named,      fc::ovariant()},
                               {"maximum_share_supply", "int64",    rpc_server::optional_named,      fc::variant(BTS_BLOCKCHAIN_MAX_SHARES)}
                              },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
          )" };
    fc::variant rpc_server_impl::wallet_asset_create(const fc::variants& params)
    {
       fc::variant_object named_params = params[3].get_object();
       auto create_asset_trx = _client->wallet_asset_create(
              params[0].as_string(), 
              params[1].as_string(),
              named_params["description"].as_string(),
              named_params["data"],
              params[2].as_string(),
              named_params["maximum_share_supply"].as_int64());
       return fc::variant(create_asset_trx);
    }


    static rpc_server::method_data wallet_asset_issue_metadata{"wallet_asset_issue", nullptr,
            /* description */ "Issues new shares of a given asset type",
            /* returns: */    "invoice_summary",
            /* params:          name                    type        classification                   default value */
                              {{"amount",               "int",      rpc_server::required_positional, fc::ovariant()},
                               {"symbol",               "string",   rpc_server::required_positional, fc::ovariant()},
                               {"to_account_name",      "string",   rpc_server::required_positional, fc::ovariant()},
                              },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
          )" };
    fc::variant rpc_server_impl::wallet_asset_issue(const fc::variants& params)
    {
       fc::variant_object named_params = params[3].get_object();
       auto issue_asset_trx = _client->wallet_asset_issue(
              params[0].as_int64(),
              params[1].as_string(),
              params[2].as_string() );
       _client->broadcast_transaction( issue_asset_trx );
       return fc::variant(issue_asset_trx);
    }

    static rpc_server::method_data wallet_list_contact_accounts_metadata{"wallet_list_contact_accounts", nullptr,
            /* description */ "Lists all foreign addresses and their labels associated with this wallet",
            /* returns: */    "map<string,extended_address>",
            /* params:          name     type    classification                   default value */
                              {},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_list_contact_accounts(const fc::variants& params)
    {  try {
      auto accounts = _client->wallet_list_contact_accounts();
      return fc::variant( accounts );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data wallet_list_reserved_names_metadata{"wallet_list_reserved_names", nullptr,
            /* description */ "Lists all reserved names controlled by this wallet, filtered by account",
            /* returns: */    "vector<name_record>",
            /* params:          name            type       classification                   default value */
                              {{"account_name", "string",  rpc_server::optional_positional, fc::variant("*")}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_list_reserved_names(const fc::variants& params)
    {  try {
      std::string account_name = params[0].as_string();
      std::vector<name_record> name_records = _client->wallet_list_reserved_names(account_name);
      return fc::variant( name_records );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data wallet_rename_account_metadata{"wallet_rename_account", nullptr,
            /* description */ "Rename an account in wallet",
            /* returns: */    "void",
            /* params:          name                    type       classification                   default value */
                              {{"current_account_name", "string",  rpc_server::required_positional, fc::ovariant()},
                               {"new_account_name",     "string",  rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
             Note: new_account_name must be unique or this call will throw an exception.
     )" };
    fc::variant rpc_server_impl::wallet_rename_account(const fc::variants& params)
    {  try {
      std::string current_account_name = params[0].as_string();
      std::string new_account_name = params[1].as_string();
      _client->wallet_rename_account(current_account_name, new_account_name);
      return fc::variant();
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }


    static rpc_server::method_data wallet_list_receive_accounts_metadata{"wallet_list_receive_accounts", nullptr,
            /* description */ "Lists all foreign addresses and their labels associated with this wallet",
            /* returns: */    "map<string,extended_address>",
            /* params:          name     type    classification                   default value */
                              {},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_list_receive_accounts(const fc::variants& params)
    {  try {
      auto accounts = _client->wallet_list_receive_accounts();
      return fc::variant( accounts );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data wallet_get_account_metadata{"wallet_get_account", nullptr,
            /* description */ "Lists all foreign addresses and their labels associated with this wallet",
            /* returns: */    "account_record",
            /* params:          name                  type                   classification                   default_value */
                              {{"account_name",       "string",              rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_get_account(const fc::variants& params)
    {  try {
      auto account_record = _client->wallet_get_account(params[0].as_string());
      return fc::variant( account_record );
      /*
      fc::mutable_variant_object result;
      result["name"] = account_record.name;
      result["extended_address"] = extended_address( account_record.extended_key );
      return fc::variant( std::move(result) );
      */
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }


    static rpc_server::method_data wallet_get_balance_metadata{"wallet_get_balance", nullptr,
            /* description */ "Returns the wallet's current balance",
            /* returns: */    "balances",
            /* params:          name                  type      classification                   default_value */
                              {
                               {"asset_symbol",       "string", rpc_server::optional_positional, fc::variant(BTS_ADDRESS_PREFIX)},
                               {"account_name",       "string", rpc_server::optional_positional, fc::variant("*")}
                              },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )",
    /*aliases*/ { "bitcoin_getbalance", "getbalance", "balance" }};
    fc::variant rpc_server_impl::wallet_get_balance(const fc::variants& params)
    {
      return fc::variant(_client->wallet_get_balance( params[0].as_string(), params[1].as_string() ));
    }

    static rpc_server::method_data wallet_get_transaction_history_metadata{"wallet_get_transaction_history", nullptr,
            /* description */ "Retrieves all transactions into or out of this wallet",
            /* returns: */    "std::vector<wallet_transaction_record>",
            /* params:          name     type      classification                   default_value */
                              {{"count", "unsigned",    rpc_server::optional_positional, 0}},
            /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_get_transaction_history(const fc::variants& params)
    {
      unsigned count = params[0].as<unsigned>();
      return fc::variant( _client->wallet_get_transaction_history( count ) );
    }

    static rpc_server::method_data wallet_get_transaction_history_summary_metadata{"wallet_get_transaction_history_summary", nullptr,
     /* description */ "Returns a transaction history table for pretty printing",
     /* returns: */ "std::vector<pretty_transaction>",
     /* params: name type classification default_value */
                       {{"count", "unsigned", rpc_server::optional_positional, 0}},
     /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
     R"(
     )" };
    fc::variant rpc_server_impl::wallet_get_transaction_history_summary(const fc::variants& params)
    {
      unsigned count = params[0].as<unsigned>();
      return fc::variant( _client->wallet_get_transaction_history_summary( count ) );
    }

    static rpc_server::method_data blockchain_get_name_record_metadata{"blockchain_get_name_record", nullptr,
            /* description */ "Retrieves the name record",
            /* returns: */    "name_record",
            /* params:          name          type      classification                   default_value */
                              {{"name",       "string", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
          )",
          /* aliases */{ "blockchain_get_name" } //deprecated name
          };
    fc::variant rpc_server_impl::blockchain_get_name_record(const fc::variants& params)
    {
      return fc::variant( _client->blockchain_get_name_record(params[0].as_string()) );
    }

    static rpc_server::method_data blockchain_get_name_record_by_id_metadata{"blockchain_get_name_record_by_id", nullptr,
            /* description */ "Retrieves the name record for the given name_id",
            /* returns: */    "name_record",
            /* params:          name          type      classification                   default_value */
                              {{"name_id",    "int", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
          )",
          /* aliases */{ "blockchain_get_name_by_id" } //deprecated name
          };
    fc::variant rpc_server_impl::blockchain_get_name_record_by_id(const fc::variants& params)
    {
      return fc::variant( _client->blockchain_get_name_record_by_id(params[0].as<int32_t>()) );
    }

    static rpc_server::method_data blockchain_get_asset_record_metadata{"blockchain_get_asset_record", nullptr,
            /* description */ "Retrieves the asset record by the ticker symbol",
            /* returns: */    "asset_record",
            /* params:          asset          type      classification                   default_value */
                              {{"symbol",       "string", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
          )",
          /* aliases */{ "blockchain_get_asset" } //deprecated name
          };
    fc::variant rpc_server_impl::blockchain_get_asset_record(const fc::variants& params)
    {
      oasset_record rec = _client->blockchain_get_asset_record(params[0].as_string());
      if( !!rec )
         return fc::variant( *rec );
      return fc::variant();
    }

    static rpc_server::method_data blockchain_get_asset_record_by_id_metadata{"blockchain_get_asset_record_by_id", nullptr,
            /* description */ "Retrieves the asset record by the ticker symbol",
            /* returns: */    "asset_record",
            /* params:          asset          type      classification                   default_value */
                              {{"asset_id",       "int", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
          )",
          /* aliases */{ "blockchain_get_asset_by_id" } //deprecated name
          };
    fc::variant rpc_server_impl::blockchain_get_asset_record_by_id(const fc::variants& params)
    {
      oasset_record rec = _client->blockchain_get_asset_record_by_id(params[0].as<int32_t>());
      if( !!rec )
         return fc::variant( *rec );
      return fc::variant();
    }

    static rpc_server::method_data wallet_register_account_metadata{"wallet_register_account", nullptr,
            /* description */ "Register an account with the blockchain", 
            /* returns: */    "signed_transaction",
            /* params:          name             type       classification                   default_value */
                              {{"name",          "string",  rpc_server::required_positional, fc::ovariant()},
                               {"data",          "variant", rpc_server::optional_positional, fc::ovariant()},
                               {"as_delegate",   "bool",    rpc_server::optional_positional, fc::variant(false)}},
            /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
          R"(
          )"
     };
    fc::variant rpc_server_impl::wallet_register_account(const fc::variants& params)
    {
       return fc::variant( _client->wallet_register_account(params[0].as_string(), params[1]) );
    }


    static rpc_server::method_data wallet_submit_proposal_metadata{ "wallet_submit_proposal", nullptr,
      /* description */ "Submit a proposal to the delegates for voting",
      /* returns: */    "transaction_id",
      /* params:          name          type       classification                   default_value */
      { { "proposer", "string", rpc_server::required_positional, fc::ovariant() },
        { "subject", "string", rpc_server::required_positional, fc::ovariant() },
        { "body", "string", rpc_server::required_positional, fc::ovariant() },
        { "proposal_type", "string", rpc_server::required_positional, fc::ovariant() },
        { "json_data", "variant", rpc_server::optional_positional, fc::ovariant() } 
      },
        /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
      R"(
     )" };
    fc::variant rpc_server_impl::wallet_submit_proposal(const fc::variants& params)
    {
      auto transaction_id = _client->wallet_submit_proposal(params[0].as_string(), //proposer 
                                                            params[1].as_string(), //subject
                                                            params[2].as_string(), //body
                                                            params[3].as_string(), //proposal_type
                                                            params[4]              //json_data
                                                            );
      return fc::variant(transaction_id);
    }

    static rpc_server::method_data wallet_vote_proposal_metadata{ "wallet_vote_proposal", nullptr,
      /* description */ "Vote on a proposal",
      /* returns: */    "transaction_id",
      /* params:          name          type       classification                   default_value */
      { { "name", "string", rpc_server::required_positional, fc::ovariant() },
        { "proposal_id", "transaction_id", rpc_server::required_positional, fc::ovariant() },
        { "vote", "unsigned integer", rpc_server::required_positional, fc::ovariant() }
      },
      /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked | rpc_server::connected_to_network,
      R"(
     )" };
    fc::variant rpc_server_impl::wallet_vote_proposal(const fc::variants& params)
    {
      auto transaction_id = _client->wallet_vote_proposal(params[0].as_string(), 
                                                          params[1].as<int32_t>(),
                                                          params[2].as<uint8_t>());
      return fc::variant(transaction_id);
    }


    static rpc_server::method_data wallet_set_delegate_trust_status_metadata{ "wallet_set_delegate_trust_status", nullptr,
      /* description */ "Sets the trust level for a delegate",
      /* returns: */    "null",
      /* params:          name          type       classification                   default_value */
      { { "delegate_name", "string", rpc_server::required_positional, fc::ovariant() },
      { "trust_level", "integer", rpc_server::required_positional, fc::ovariant() } },
      /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
      R"(
returns false if delegate is not recognized
     )" };
    fc::variant rpc_server_impl::wallet_set_delegate_trust_status(const fc::variants& params)
    {
      _client->wallet_set_delegate_trust_status(params[0].as_string(), params[1].as<int32_t>());
      return fc::variant();
    }

    static rpc_server::method_data wallet_get_delegate_trust_status_metadata{ "wallet_get_delegate_trust_status", nullptr,
      /* description */ "Gets the trust level for a delegate",
      /* returns: */    "delegate_trust_status",
      /* params:          name          type       classification                   default_value */
      { { "delegate_name", "string", rpc_server::required_positional, fc::ovariant() } },
      /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
      R"(
returns false if delegate is not recognized
     )" };
    fc::variant rpc_server_impl::wallet_get_delegate_trust_status(const fc::variants& params)
    {
      return fc::variant();//_client->wallet_get_delegate_trust_status(params[0].as_string()));
    }

    static rpc_server::method_data wallet_list_delegate_trust_status_metadata{ "wallet_list_delegate_trust_status", nullptr,
      /* description */ "List the trust status for delegates",
      /* returns: */    "map<delegate_name,delegate_trust_status>",
      /* params:          name          type       classification                   default_value */
      { },
      /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
      R"(
     )" };
    fc::variant rpc_server_impl::wallet_list_delegate_trust_status(const fc::variants& params)
    {
      return fc::variant();//_client->wallet_list_delegate_trust_status());
    }

    static rpc_server::method_data blockchain_get_transaction_metadata{ "blockchain_get_transaction", nullptr,
            /* description */ "Get detailed information about an in-wallet transaction",
            /* returns: */    "signed_transaction",
            /* params:          name              type              classification                   default_value */
                              {{"transaction_id", "string", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
          )" };
    fc::variant rpc_server_impl::blockchain_get_transaction(const fc::variants& params)
    {
      return fc::variant( _client->blockchain_get_transaction( params[0].as<transaction_id_type>() )  );
    }

    static rpc_server::method_data blockchain_get_block_metadata{"blockchain_get_block", nullptr,
            /* description */ "Retrieves the block header for the given block hash",
            /* returns: */    "block_header",
            /* params:          name              type             classification                   default_value */
                              {{"block_hash",     "block_id_type", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
     )",
    /*aliases*/ { "bitcoin_getblock", "getblock" }};
    fc::variant rpc_server_impl::blockchain_get_block(const fc::variants& params)
    { try {
      return fc::variant( _client->blockchain_get_block( params[0].as<block_id_type>() )  );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data blockchain_get_block_by_number_metadata{"blockchain_get_block_by_number", nullptr,
                                   /* description */ "Retrieves the block header for the given block number",
                                   /* returns: */    "block_header",
                                   /* params:          name              type     classification                   default_value */
                                                     {{"block_number",   "int32", rpc_server::required_positional, fc::ovariant()}},
                                 /* prerequisites */ rpc_server::json_authenticated};
    fc::variant rpc_server_impl::blockchain_get_block_by_number(const fc::variants& params)
    { try {
      return fc::variant( _client->blockchain_get_block_by_number( params[0].as<uint32_t>() ) );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("params",params) ) }

    static rpc_server::method_data validate_address_metadata{"validate_address", nullptr,
            /* description */ "Return information about given BitShares address",
            /* returns: */    "bool",
            /* params:          name              type       classification                   default_value */
                              {{"address",        "address", rpc_server::required_positional, fc::ovariant()}},
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
      FC_ASSERT("Not implemented")
      try {
        return fc::variant(params[0].as_string());
      }
      catch ( const fc::exception& )
      {
        return fc::variant(false);
      }
    }

    static rpc_server::method_data wallet_rescan_blockchain_metadata{"wallet_rescan_blockchain", nullptr,
            /* description */ "Rescan the block chain from the given block number",
            /* returns: */    "void",
            /* params:          name              type   classification                   default_value */
                              {{"starting_block_number", "int", rpc_server::optional_positional, 0}},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_rescan_blockchain(const fc::variants& params)
    {
      uint32_t starting_block_number = params[0].as<uint32_t>();;
      _client->wallet_rescan_blockchain(starting_block_number);
      return fc::variant();
    }

    static rpc_server::method_data wallet_rescan_blockchain_state_metadata{"wallet_rescan_blockchain_state", nullptr,
            /* description */ "Rescans the genesis block and state (not the transactions)",
            /* returns: */    "void",
            /* params:          name              type    required */
                              {},
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_rescan_blockchain_state(const fc::variants& params)
    {
      _client->wallet_rescan_blockchain_state();
      return fc::variant();
    }

    static rpc_server::method_data wallet_import_bitcoin_metadata{"wallet_import_bitcoin", nullptr,
            /* description */ "Import a BTC/PTS wallet",
            /* returns: */    "void",
            /* params:          name           type      classification                          default_value */
                              {
                               {"filename",   "path",   rpc_server::required_positional,        fc::ovariant()},
                               {"account_name", "string", rpc_server::required_positional,      fc::ovariant()},
                               {"passphrase", "string", rpc_server::required_positional_hidden, fc::ovariant()}
                              },
          /* prerequisites */ rpc_server::json_authenticated | rpc_server::wallet_open | rpc_server::wallet_unlocked,
          R"(
     )" };
    fc::variant rpc_server_impl::wallet_import_bitcoin(const fc::variants& params)
    {
        auto filename = params[0].as<fc::path>();
        auto passphrase = params[1].as<std::string>();
        auto account_name = params[2].as<std::string>();
        try
        {
          _client->wallet_import_bitcoin( filename, passphrase, account_name );
        }
        catch( const fc::exception& e ) // TODO: see wallet_unlock()
        {
          wlog( "${e}", ("e",e.to_detail_string() ) );
          throw rpc_wallet_passphrase_incorrect_exception();
        }
        return fc::variant();
    }

    // TODO: get account argument
    static rpc_server::method_data wallet_import_private_key_metadata{"wallet_import_private_key", nullptr,
            /* description */ "Import a BTC/PTS private key in wallet import format (WIF)",
            /* returns: */    "void",
            /* params:          name            type           classification                   default_value */
                              {{"key_to_import", "wif_private_key", rpc_server::required_positional, fc::ovariant()},
                               {"account_name", "string",      rpc_server::optional_positional, fc::variant("default")},
                               {"rescan",       "bool",        rpc_server::optional_positional, false}},
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
      auto key_to_import   =  params[0].as_string();
      std::string account_name = params[1].as_string();
      bool wallet_rescan_blockchain = params[2].as_bool();
      _client->wallet_import_private_key(key_to_import, account_name, wallet_rescan_blockchain);
      return fc::variant();
    }

    static rpc_server::method_data blockchain_get_names_metadata{"blockchain_get_names", nullptr,
            /* description */ "Returns the list of reserved names sorted alphabetically",
            /* returns: */    "vector<name_record>",
            /* params:          name     type      classification                   default value */
                              {{"first", "string", rpc_server::optional_positional, fc::variant("")},
                               {"count", "int",    rpc_server::optional_positional, -1}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
blockchain_get_names (first, count)

Returns up to count reserved names that follow first alphabetically.
             )" };
    fc::variant rpc_server_impl::blockchain_get_names(const fc::variants& params)
    {
      std::string first = params[0].as_string();
      uint32_t count = params[1].as<uint32_t>();
      return fc::variant(_client->blockchain_get_names( first, count ) );
    }



    static rpc_server::method_data blockchain_get_assets_metadata{"blockchain_get_assets", nullptr,
            /* description */ "Returns the list of reserved assets sorted alphabetically",
            /* returns: */    "vector<asset_record>",
            /* params:          name     type      classification                   default value */
                              {{"first_symbol", "string", rpc_server::optional_positional, fc::variant("")},
                               {"count", "int",    rpc_server::optional_positional, -1}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
blockchain_get_assets (first, count)

Returns up to count reserved assets that follow first alphabetically.
             )" };
    fc::variant rpc_server_impl::blockchain_get_assets(const fc::variants& params)
    {
      std::string first_symbol = params[0].as_string();
      uint32_t count = params[1].as<uint32_t>();
      return fc::variant( _client->blockchain_get_assets(first_symbol, count) );
    }


    static rpc_server::method_data blockchain_get_delegates_metadata{"blockchain_get_delegates", nullptr,
            /* description */ "Returns the list of delegates sorted by vote",
            /* returns: */    "vector<name_record>",
            /* params:          name     type      classification                   default value */
                              {{"first", "int",    rpc_server::optional_positional, 0},
                               {"count", "int",    rpc_server::optional_positional, -1}},
          /* prerequisites */ rpc_server::json_authenticated,
          R"(
blockchain_get_delegates (start, count)

Returns information about the delegates sorted by their net votes starting from position start and returning up to count items.

Arguments:
   first - the index of the first delegate to be returned
   count - the maximum number of delegates to be returned

             )" };
    fc::variant rpc_server_impl::blockchain_get_delegates(const fc::variants& params)
    {
      uint32_t first = params[0].as<uint32_t>();;
      uint32_t count = params[1].as<uint32_t>();
      std::vector<name_record> delegate_records = _client->blockchain_get_delegates(first, count);
      return fc::variant(delegate_records);
    }

    static rpc_server::method_data network_get_connection_count_metadata{"network_get_connection_count", nullptr,
            /* description */ "Returns the number of connections to other nodes",
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
      return fc::variant(_client->network_get_connection_count());
    }

    static rpc_server::method_data network_get_peer_info_metadata{"network_get_peer_info", nullptr,
            /* description */ "Returns data about each connected node",
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
      return _client->network_get_peer_info();
    }

    static rpc_server::method_data network_set_advanced_node_parameters_metadata{"network_set_advanced_node_parameters", nullptr,
            /* description */ "Sets advanced node parameters, used for setting up automated tests",
            /* returns: */    "null",
            /* params:          name      type         classification                   default value */
                              {{"params", "jsonobject",rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
  R"(
Result:
null
  )" };
    fc::variant rpc_server_impl::network_set_advanced_node_parameters(const fc::variants& params)
    {
      _client->network_set_advanced_node_parameters(params[0].get_object());
      return fc::variant();
    }

    static rpc_server::method_data network_add_node_metadata{"network_add_node", nullptr,
            /* description */ "Attempts add or remove <node> from the peer list or try a connection to <node> once",
            /* returns: */    "null",
            /* params:          name      type       classification                   default value */
                              {{"node",   "string",  rpc_server::required_positional, fc::ovariant()},
                               {"command","string",  rpc_server::required_positional, fc::ovariant()}},
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
      _client->network_add_node(fc::ip::endpoint::from_string(params[0].as_string()), params[1].as_string());
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
      if (_on_quit_promise)
        _on_quit_promise->set_value();
      _client->stop();
      return fc::variant();
    }

    static rpc_server::method_data network_get_transaction_propagation_data_metadata{"network_get_transaction_propagation_data", nullptr,
            /* description */ "Returns the time the transaction was first seen by this client",
            /* returns: */    "bts::net::message_propagation_data",
            /* params:          name              type             classification                   default value */
                              {{"transaction_id", "transaction_id",rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
network_get_transaction_propagation_data <transaction_id>

Returns the time the transaction was first seen by this client.

This interrogates the p2p node's message cache to find out when it first saw this transaction.
The data in the message cache is only kept for a few blocks, so you can only use this to ask
about recent transactions. This is intended to be used to track message propagation delays
in our test network.
)" };
    fc::variant rpc_server_impl::network_get_transaction_propagation_data(const fc::variants& params)
    {
      return fc::variant(_client->network_get_transaction_propagation_data(params[0].as<transaction_id_type>()));
    }

    static rpc_server::method_data network_get_block_propagation_data_metadata{"network_get_block_propagation_data", nullptr,
            /* description */ "Returns the time the block was first seen by this client",
            /* returns: */    "bts::net::message_propagation_data",
            /* params:          name              type             classification                   default value */
                              {{"block_hash",     "block_id_type", rpc_server::required_positional, fc::ovariant()}},
          /* prerequisites */ rpc_server::json_authenticated,
R"(
network_get_block_propagation_data <block_hash>

Returns the time the block was first seen by this client.

This interrogates the p2p node's message cache to find out when it first saw this transaction.
The data in the message cache is only kept for a few blocks, so you can only use this to ask
about recent transactions. This is intended to be used to track message propagation delays
in our test network.
)" };
    fc::variant rpc_server_impl::network_get_block_propagation_data(const fc::variants& params)
    {
      return fc::variant(_client->network_get_block_propagation_data(params[0].as<block_id_type>()));
    }

    static rpc_server::method_data _list_json_commands_metadata{"_list_json_commands", nullptr,
        /* description */ "Lists commands",
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
         if( my->_on_quit_promise && !my->_on_quit_promise->ready() )
            my->_on_quit_promise->set_value();
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

  bool rpc_server::configure( const rpc_server::config& cfg )
  {
    if (!cfg.is_valid())
      return false;
    try
    {
      my->_config = cfg;
      my->_tcp_serv.listen( cfg.rpc_endpoint );
      ilog( "listening for json rpc connections on port ${port}", ("port",my->_tcp_serv.get_port()) );

      my->_accept_loop_complete = fc::async( [=]{ my->accept_loop(); } );


      auto m = my.get();
      my->_httpd.listen(cfg.httpd_endpoint);
      my->_httpd.on_request( [m]( const fc::http::request& r, const fc::http::server::response& s ){ m->handle_request( r, s ); } );

      return true;
    } FC_RETHROW_EXCEPTIONS( warn, "attempting to configure rpc server ${port}", ("port",cfg.rpc_endpoint)("config",cfg) );
  }

  fc::variant rpc_server::direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
  {
    return my->direct_invoke_method(method_name, arguments);
  }

  const rpc_server::method_data& rpc_server::get_method_data(const std::string& method_name)
  {
    auto iter = my->_alias_map.find(method_name);
    if (iter != my->_alias_map.end())
      return my->_method_map[iter->second];
    FC_THROW_EXCEPTION(key_not_found_exception, "Method \"${name}\" not found", ("name", method_name));
  }

  void rpc_server::validate_method_data(method_data method)
  {
    bool encountered_default_argument = false;
    bool encountered_optional_argument = false;
    bool encountered_named_argument = false;

    for (const rpc_server::parameter_data& parameter : method.parameters)
    {
      switch (parameter.classification)
      {
          case rpc_server::required_positional:
          case rpc_server::required_positional_hidden:
            // can't have any required arguments after an optional argument
            FC_ASSERT(!encountered_optional_argument);
            // required arguments can't have a default value
            FC_ASSERT(!parameter.default_value);
            break;
          case rpc_server::optional_positional:
            // can't have any positional optional arguments after a named argument
            FC_ASSERT(!encountered_named_argument);
            // if previous arguments have a default value, this one must too
            if (encountered_default_argument)
              FC_ASSERT(parameter.default_value);
            encountered_optional_argument = true;
            if( parameter.default_value.valid() )
              encountered_default_argument = true;
            break;
          case rpc_server::optional_named:
            encountered_optional_argument = true;
            encountered_named_argument = true;
            break;
          default:
            FC_ASSERT(false, "Invalid parameter classification");
            break;
      }
    }
  }

  void rpc_server::register_method(method_data data)
  {
    validate_method_data(data);
    FC_ASSERT(my->_alias_map.find(data.name) == my->_alias_map.end(), "attempting to register an exsiting method name ${m}", ("m", data.name));
    my->_alias_map[data.name] = data.name;
    for ( auto alias : data.aliases )
    {
        FC_ASSERT(my->_alias_map.find(alias) == my->_alias_map.end(), "attempting to register an exsiting method name ${m}", ("m", alias));
        my->_alias_map[alias] = data.name;
    }
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

  void rpc_server::wait_on_quit()
  {
     my->_on_quit_promise.reset( new fc::promise<void>("rpc_quit") );
     my->_on_quit_promise->wait();
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
