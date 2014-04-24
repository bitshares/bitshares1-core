#include <bts/rpc/rpc_server.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <fc/network/http/server.hpp>
#include <fc/interprocess/file_mapping.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/join.hpp>
#include <sstream>
#include <limits>


namespace bts { namespace rpc { 

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
         std::string         _username;

         typedef std::map<std::string, rpc_server::method_data> method_map_type;
         method_map_type _method_map;

         /** the set of connections that have successfully logged in */
         std::unordered_set<fc::rpc::json_connection*> _authenticated_connection_set;

         void handle_request( const fc::http::request& r, const fc::http::server::response& s )
         {
             s.add_header( "Connection", "close" );
             ilog( "handle request ${r}", ("r",r.path) );

             try {
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
                ilog( "username: '${u}' password: '${p}' config ${c}", ("u",username)("p",password)("c",_config) );
                if( _config.rpc_user     != username ||
                    _config.rpc_password != password )
                {
                   s.add_header( "WWW-Authenticate", "Basic realm=\"bts wallet\"" );
                   std::string message = "Unauthorized";
                   s.set_length( message.size() );
                   s.set_status( fc::http::reply::NotAuthorized );
                   s.write( message.c_str(), message.size() );
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

         void handle_http_wallet(const fc::http::request& r, const fc::http::server::response& s )
         {
              std::stringstream ss;
              ss << "<html><head><title>BitShares XT Wallet</title>";
              ss << "</head>";
              ss << "<script type=\"text/javascript\" charset=\"utf-8\" src=\"http://code.jquery.com/jquery-1.7.2.min.js\"></script>"
                << "<script type=\"text/javascript\" src=\"/jquery.jsonrpc.js\"></script>\n" 
                << "<script type=\"text/javascript\">"
                << " $(document).ready(function() {"
                << "      $.jsonRPC.setup({"
                << "          endPoint : 'http://localhost:9989/rpc',"
                << "          namespace : ''"
                << "     });"
                << "      $(\"input#transfer\").click(function() {"
                << "          $.jsonRPC.request('sendtoaddress', {"
                << "              params : [$(\"text#amount\").val(), $(\"text#payto\").val(), $(\"text#memo\").val()],"
                << "              success : function(data) {"
                << "                 $(\"<p />\").text(data.result).appendTo($(\"p#result\"));"
                << "              },"
                << "              error : function(data) {"
                << "                  $(\"<p />\").text(data.error.message).appendTo($(\"p#result\"));"
                << "              }"
                << "         });"
                << "     });"
                << "  });"
               << " </script> ";

              ss << "<body>\n";
              ss << "  <h1>BitShares XT Wallet</h1>\n";
              ss << "  <hr/>\n";
              ss << "  <h3>Balance: " << _client->get_wallet()->get_balance(0).get_rounded_amount() << "</h3>";
              ss << "  <hr/>\n";

              ss << "  <div id=\"transfer\"><form>\n";
              ss << "     <h2>Transfer</h2> \n";
              ss << "     Pay to: <input type=\"text\" placeholder=\"Address\" id=\"payto\"/>\n";
              ss << "     Amount: <input type=\"text\" placeholder=\"0.0\" id=\"amount\"/>\n";
              ss << "     Memo: <input type=\"text\" placeholder=\"Memo\" id=\"memo\" />\n";
              ss << "     <input type=\"button\" id=\"transfer\" value=\"Transfer\"/>\n";
              ss << "     </form>\n";
              ss << "     <p id=\"result\"> </p>\n";
              ss << "  </div>\n";

              ss << "  <hr/>";
              ss << "  <h3>Transaction History</h3>\n";
              ss << "  <table width=\"100%\">\n";
              ss << "  <tr><th>Date</th><th>Type</th><th>Address</th><th>Amount</th></tr>\n";
              ss << "  </table>\n";
              ss << "</body></html>";

              auto result = ss.str();

              s.set_status( fc::http::reply::OK );
              s.set_length( result.size() );
              s.write( result.c_str(), result.size() );
         }

         void handle_http_rpc(const fc::http::request& r, const fc::http::server::response& s )
         {
                std::string str(r.body.data(),r.body.size());
                try {
                   auto rpc_call = fc::json::from_string( str ).get_object();
                   auto method_name = rpc_call["method"].as_string();
                   auto params = rpc_call["params"].get_array();

                   ilog( "method: ${m}  params: ${p}", ("m", method_name)("p",params) );
                   ilog( "call: ${c}", ("c", str) );
                   
                   auto call_itr = _method_map.find( method_name );
                   if( call_itr != _method_map.end() )
                   {
                      fc::mutable_variant_object  result;
                      result["id"]     =  rpc_call["id"];
                      try {
                         result["result"] =  dispatch_authenticated_method(call_itr->second, params);
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
                   }
                   else
                   {
                       s.set_status( fc::http::reply::NotFound );
                       s.set_length( 9 );
                       s.write( "Invalid Method", 9 );
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
                    ilog( "${e}", ("e",message) );
                }
                catch (...)
                {
                    std::string message = "Invalid RPC Request\n";
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::BadRequest );
                    s.write( message.c_str(), message.size() );
                    ilog( "${e}", ("e",message) );
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
          if ((method_data.prerequisites & rpc_server::json_authenticated) &&
              _authenticated_connection_set.find(con) == _authenticated_connection_set.end())
            FC_THROW_EXCEPTION(exception, "not logged in"); 
          return dispatch_authenticated_method(method_data, arguments);
        }

        fc::variant dispatch_authenticated_method(const rpc_server::method_data& method_data, 
                                                  const fc::variants& arguments)
        {
          if (method_data.prerequisites & rpc_server::wallet_open)
            check_wallet_is_open();
          if (method_data.prerequisites & rpc_server::wallet_unlocked)
            check_wallet_unlocked();
          if (method_data.prerequisites & rpc_server::connected_to_network)
            check_connected_to_network();
          if (arguments.size() > method_data.parameters.size())
            FC_THROW_EXCEPTION(exception, "too many arguments (expected at most ${count})", ("count", method_data.parameters.size())); 
          uint32_t required_argument_count = 0;
          for (const rpc_server::parameter_data& parameter : method_data.parameters)
          {
            if (parameter.required)
              ++required_argument_count;
            else
              break;
          }
          if (arguments.size() < required_argument_count)
            FC_THROW_EXCEPTION(exception, "too many arguments (expected at leasst ${count})", ("count", required_argument_count));

          return method_data.method(arguments);
        }

        // This method invokes the function directly, called by the CLI intepreter.
        fc::variant direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
        {
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
        fc::variant help( const fc::variants& params );
        fc::variant openwallet( const fc::variants& params );
        fc::variant createwallet( const fc::variants& params );
        fc::variant currentwallet( const fc::variants& params );
        fc::variant closewallet( const fc::variants& params );
        fc::variant walletpassphrase( const fc::variants& params );
        fc::variant getnewaddress( const fc::variants& params );
        fc::variant _create_sendtoaddress_transaction( const fc::variants& params );
        fc::variant sendtransaction( const fc::variants& params );
        fc::variant sendtoaddress( const fc::variants& params );
        fc::variant listrecvaddresses( const fc::variants& params );
        fc::variant getbalance( const fc::variants& params );
        fc::variant get_transaction( const fc::variants& params );
        fc::variant getblock( const fc::variants& params );
        fc::variant validateaddress( const fc::variants& params );
        fc::variant rescan( const fc::variants& params );
        fc::variant import_bitcoin_wallet( const fc::variants& params );
        fc::variant import_private_key( const fc::variants& params );
    };

    fc::variant rpc_server_impl::login(fc::rpc::json_connection* json_connection, const fc::variants& params)
    {
      FC_ASSERT( params.size() == 2 );
      FC_ASSERT( params[0].as_string() == _config.rpc_user )
      FC_ASSERT( params[1].as_string() == _config.rpc_password )
      _authenticated_connection_set.insert( json_connection );
      return fc::variant( true );
    }

    fc::variant rpc_server_impl::help(const fc::variants& params)
    {
      FC_ASSERT( params.size() == 0 );
      std::vector<std::vector<std::string> > help_strings;
      for (const method_map_type::value_type& value : _method_map)
      {
        if (value.second.name[0] != '_') // hide undocumented commands
        {
          std::vector<std::string> parameter_strings;
          for (const rpc_server::parameter_data& parameter : value.second.parameters)
          {
            if (parameter.required)
              parameter_strings.push_back(std::string("<") + parameter.name + std::string(">"));
            else
              parameter_strings.push_back(std::string("[") + parameter.name + std::string("]"));
          }
          help_strings.emplace_back(std::vector<std::string>{value.second.name, 
                                                             boost::algorithm::join(parameter_strings, " "), 
                                                             value.second.description});
        }
      }
      return fc::variant( help_strings );
    }

    fc::variant rpc_server_impl::openwallet(const fc::variants& params)
    {
       std::string username, passphrase;
      if( params.size() == 0 )
      {
         username = "default";
      }
      else if( params.size() == 2 )
      {
         username   = params[0].as_string();
         if( !params[1].is_null() )
            passphrase = params[1].as_string();
      }
      else
      {
         FC_ASSERT( params.size() == 0 || params.size() == 2, "[username,password]" );
      }
       
      try
      {
        _username = username;
        _client->get_wallet()->open( _client->get_data_dir() / (username + "_wallet.dat"), passphrase );
        return fc::variant(true);
      }
      catch( const fc::exception& e )
      {
         wlog( "${e}", ("e",e.to_detail_string() ) );
         throw;
      }
      catch (...) // TODO: this is an invalid conversion to rpc_wallet_passphrase exception...
      {           //       if the problem is 'file not found' or 'invalid user' or 'permission denined'
                  //       or some other filesystem error then it should be properly reported.
        throw rpc_wallet_passphrase_incorrect_exception();
      }          
    }

    fc::variant rpc_server_impl::createwallet(const fc::variants& params)
    {
       std::string username = "default";
       std::string passphrase = "", keypassword;

       FC_ASSERT( params.size() == 3, "username, password, keypassphrase" );

       keypassword = params[0].as_string();
       if( !params[0].is_null() ) username      = params[0].as_string();
       if( !params[1].is_null() ) passphrase    = params[1].as_string();
       if( !params[2].is_null() ) keypassword   = params[2].as_string();
       
      try
      {
        _username = username;
        _client->get_wallet()->create( _client->get_data_dir() / (username + "_wallet.dat"), 
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

    fc::variant rpc_server_impl::currentwallet(const fc::variants& params)
    {
       FC_ASSERT( params.size() == 0, "expected parameters: []" )
       if( !_client->get_wallet()->is_open() )
          return fc::variant(nullptr);
       return fc::variant(_username);
    }

    fc::variant rpc_server_impl::closewallet(const fc::variants& params)
    {
       FC_ASSERT( params.size() == 0, "expected parameters: []" )
       return fc::variant( _client->get_wallet()->close() );
    }

    fc::variant rpc_server_impl::walletpassphrase(const fc::variants& params)
    {
      std::string passphrase = params[0].as_string();
      try
      {
        _client->get_wallet()->unlock_wallet(passphrase);
        return fc::variant(true);
      }
      catch (...)
      {
        throw rpc_wallet_passphrase_incorrect_exception();
      }          
    }
    fc::variant rpc_server_impl::getnewaddress(const fc::variants& params)
    {
      std::string account;
      if (params.size() == 1)
        account = params[0].as_string();
      bts::blockchain::address new_address = _client->get_wallet()->new_receive_address(account);
      return fc::variant(new_address);
    }
    fc::variant rpc_server_impl::_create_sendtoaddress_transaction(const fc::variants& params)
    {
      bts::blockchain::address destination_address = params[0].as<bts::blockchain::address>();
      bts::blockchain::asset amount = params[1].as<bts::blockchain::asset>();
      std::string comment;
      if (params.size() >= 3)
        comment = params[2].as_string();
      // TODO: we're currently ignoring optional parameter 4, [to-comment]
      return fc::variant(_client->get_wallet()->transfer(amount, destination_address, comment));
    }
    fc::variant rpc_server_impl::sendtransaction(const fc::variants& params)
    {
      bts::blockchain::signed_transaction transaction = params[0].as<bts::blockchain::signed_transaction>();
      _client->broadcast_transaction(transaction);
      return fc::variant(transaction.id());
    }
    fc::variant rpc_server_impl::sendtoaddress(const fc::variants& params)
    {
      bts::blockchain::address destination_address = params[0].as<bts::blockchain::address>();
      bts::blockchain::asset amount = params[1].as<bts::blockchain::asset>();
      std::string comment;
      if (params.size() >= 3)
        comment = params[2].as_string();
      // TODO: we're currently ignoring optional 4, [to-comment]
      bts::blockchain::signed_transaction trx = _client->get_wallet()->transfer(amount, destination_address, comment);
      _client->broadcast_transaction(trx);
      return fc::variant( trx.id() ); 
    }
    fc::variant rpc_server_impl::listrecvaddresses(const fc::variants& params)
    {
      std::unordered_map<bts::blockchain::address,std::string> addresses = _client->get_wallet()->get_receive_addresses();
      return fc::variant( addresses ); 
    }
    fc::variant rpc_server_impl::getbalance(const fc::variants& params)
    {
      bts::blockchain::asset_type unit = 0;
      if (params.size() == 1)
        unit = params[0].as<bts::blockchain::asset_type>();
      return fc::variant( _client->get_wallet()->get_balance( unit ) ); 
    }
    fc::variant rpc_server_impl::get_transaction(const fc::variants& params)
    {
      return fc::variant( _client->get_chain()->fetch_transaction( params[0].as<transaction_id_type>() )  ); 
    }
    fc::variant rpc_server_impl::getblock(const fc::variants& params)
    {
      return fc::variant( _client->get_chain()->fetch_block( (uint32_t)params[0].as_int64() )  ); 
    }
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
    fc::variant rpc_server_impl::rescan(const fc::variants& params)
    {
      uint32_t block_num = 0;
      if (params.size() == 1)
        block_num = (uint32_t)params[0].as_int64();
      _client->get_wallet()->scan_chain(*_client->get_chain(), block_num);
      return fc::variant(true); 
    }
    fc::variant rpc_server_impl::import_bitcoin_wallet(const fc::variants& params)
    {
      auto wallet_dat      = params[0].as<fc::path>();
      auto wallet_password = params[1].as_string();
      _client->get_wallet()->import_bitcoin_wallet( wallet_dat, wallet_password );
      return fc::variant(true);
    }
    fc::variant rpc_server_impl::import_private_key(const fc::variants& params)
    {
      fc::sha256 hash(params[0].as_string());
      fc::ecc::private_key privkey = fc::ecc::private_key::regenerate(hash);
      _client->get_wallet()->import_key(privkey);
      _client->get_wallet()->save();
      return fc::variant(true);
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

#define JSON_METHOD_IMPL(METHODNAME) \
    boost::bind(&detail::rpc_server_impl::METHODNAME, my.get(), _1)

    method_data help_metadata{"help", JSON_METHOD_IMPL(help),
            /* description */ "list the available commands",
            /* returns: */    "bool",
            /* params:     */ {},
          /* prerequisites */ 0};
    register_method(help_metadata);

    //register_method(method_data{"login", JSON_METHOD_IMPL(login),
    //          /* description */ "authenticate JSON-RPC connection",
    //          /* returns: */    "bool",
    //          /* params:          name            type       required */ 
    //                            {{"username", "string",  true},
    //                             {"password", "string",  true}},
    //        /* prerequisites */ 0});
    //
    //

    method_data openwallet_metadata{"openwallet", JSON_METHOD_IMPL(openwallet),
                  /* description */ "unlock the wallet with the given passphrase, if no user is specified 'default' will be used.",
                  /* returns: */    "bool",
                  /* params:          name                 type      required */ 
                                    {{"user", "string", false} ,
                                     {"wallet_passphrase", "string", false}},
                /* prerequisites */ json_authenticated};
    register_method(openwallet_metadata);


    method_data createwallet_metadata{"createwallet", JSON_METHOD_IMPL(createwallet),
                  /* description */ "create a wallet with the given passphrases",
                  /* returns: */    "bool",
                  /* params:          name                 type      required */ 
                                    {
                                     {"wallet_username", "string", true},
                                     {"wallet_passphrase", "string", true},
                                     {"spending_passphrase", "string", true}
                                    },
                /* prerequisites */ json_authenticated};
    register_method(createwallet_metadata);


    method_data currentwallet_metadata{"currentwallet", JSON_METHOD_IMPL(currentwallet),
                  /* description */ "returns the username passed to openwallet",
                  /* returns: */    "string",
                  /* params:          name                 type      required */ 
                                    {},
                /* prerequisites */ };
    register_method(currentwallet_metadata);

    method_data closewallet_metadata{"closewallet", JSON_METHOD_IMPL(closewallet),
                  /* description */ "closes the curent wallet, if one is open.",
                  /* returns: */    "bool",
                  /* params:          name                 type      required */ 
                                    {},
                /* prerequisites */ };
    register_method(closewallet_metadata);

    method_data walletpassphrase_metadata{"walletpassphrase", JSON_METHOD_IMPL(walletpassphrase),
                        /* description */ "unlock the private keys in the wallet with the given passphrase",
                        /* returns: */    "bool",
                        /* params:          name                   type      required */ 
                                          {{"spending_passphrase", "string", true}, 
                                           {"timeout", "int", false} },
                      /* prerequisites */ json_authenticated | wallet_open};
    register_method(walletpassphrase_metadata);


    method_data getnewaddress_metadata{"getnewaddress", JSON_METHOD_IMPL(getnewaddress),
                     /* description */ "create a new address for receiving payments",
                     /* returns: */    "address",
                     /* params:          name       type      required */ 
                                       {{"account", "string", false}},
                   /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(getnewaddress_metadata);


    method_data sendtoaddress_metadata{"sendtoaddress", JSON_METHOD_IMPL(sendtoaddress),
                     /* description */ "Sends the given amount to the given address",
                     /* returns: */    "transaction_id",
                     /* params:          name          type       required */ 
                                       {{"to_address", "address", true},
                                        {"amount",     "asset",   true},
                                        {"comment",    "string",  false},
                                        {"to_comment", "string",  false}},
                   /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked | connected_to_network};
    register_method(sendtoaddress_metadata);

    method_data listrecvaddresses_metadata{"listrecvaddresses", JSON_METHOD_IMPL(listrecvaddresses),
                         /* description */ "Lists all receive addresses associated with this wallet",
                         /* returns: */    "vector<address>",
                         /* params:     */ {},
                       /* prerequisites */ json_authenticated | wallet_open};
    register_method(listrecvaddresses_metadata);

    method_data getbalance_metadata{"getbalance", JSON_METHOD_IMPL(getbalance),
                  /* description */ "Returns the wallet's current balance",
                  /* returns: */    "asset",
                  /* params:          name     type     required */ 
                                    {{"asset", "unit",  false}},
                /* prerequisites */ json_authenticated | wallet_open};
    register_method(getbalance_metadata);

    method_data get_transaction_metadata{"get_transaction", JSON_METHOD_IMPL(get_transaction),
                       /* description */ "Retrieves the signed transaction matching the given transaction id",
                       /* returns: */    "signed_transaction",
                       /* params:          name              type               required */ 
                                         {{"transaction_id", "transaction_id",  true}},
                     /* prerequisites */ json_authenticated};
    register_method(get_transaction_metadata);

    method_data getblock_metadata{"getblock", JSON_METHOD_IMPL(getblock),
                /* description */ "Retrieves the block header for the given block",
                /* returns: */    "block_header",
                /* params:          name              type        required */ 
                                  {{"block_number",   "uint32_t", true}},
              /* prerequisites */ json_authenticated};
    register_method(getblock_metadata);

    method_data validateaddress_metadata{"validateaddress", JSON_METHOD_IMPL(validateaddress),
                       /* description */ "Checks that the given address is valid",
                       /* returns: */    "bool",
                       /* params:          name              type       required */ 
                                         {{"address",        "address", true}},
                     /* prerequisites */ json_authenticated};
    register_method(validateaddress_metadata);

    method_data rescan_metadata{"rescan", JSON_METHOD_IMPL(rescan),
              /* description */ "Rescan the block chain from the given block",
              /* returns: */    "bool",
              /* params:          name              type    required */ 
                                {{"starting_block", "bool", false}},
            /* prerequisites */ json_authenticated | wallet_open};
    register_method(rescan_metadata);

    method_data import_bitcoin_wallet_metadata{"import_bitcoin_wallet", JSON_METHOD_IMPL(import_bitcoin_wallet),
                             /* description */ "Import a bitcoin wallet",
                             /* returns: */    "bool",
                             /* params:          name               type       required */ 
                                               {{"wallet_filename", "string",  true},
                                                 {"wallet_password", "string",  true}},
                           /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(import_bitcoin_wallet_metadata);

    method_data import_private_key_metadata{"import_private_key", JSON_METHOD_IMPL(import_private_key),
                          /* description */ "authenticate JSON-RPC connection",
                          /* returns: */    "bool",
                          /* params:          name           type            required */ 
                                            {{"private_key", "private_key",  true}},
                        /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(import_private_key_metadata);

    method_data sendtransaction_metadata{"sendtransaction", JSON_METHOD_IMPL(sendtransaction),
                       /* description */ "Broadcast a previously-created signed transaction to the network",
                       /* returns: */    "transaction_id",
                       /* params:          name                  type                   required */ 
                                         {{"signed_transaction", "signed_transaction",  true}},
                     /* prerequisites */ json_authenticated | connected_to_network};
    register_method(sendtransaction_metadata);

    method_data _create_sendtoaddress_transaction_metadata{"_create_sendtoaddress_transaction", JSON_METHOD_IMPL(_create_sendtoaddress_transaction),
                                         /* description */ "Creates a transaction in the same manner as 'sendtoaddress', but do not broadcast it",
                                         /* returns: */    "signed_transaction",
                                         /* params:          name          type       required */ 
                                                           {{"to_address", "address", true},
                                                            {"amount",     "asset",   true},
                                                            {"comment",    "string",  false},
                                                            {"to_comment", "string",  false}},
                                       /* prerequisites */ json_authenticated | wallet_open | wallet_unlocked};
    register_method(_create_sendtoaddress_transaction_metadata);
#undef JSON_METHOD_IMPL
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

#define RPC_EXCEPTION_IMPL(TYPE, ERROR_CODE, DESCRIPTION) \
  TYPE::TYPE(fc::log_message&& m) : \
    fc::exception(fc::move(m)) {} \
  TYPE::TYPE(){} \
  TYPE::TYPE(const TYPE& t) : fc::exception(t) {} \
  TYPE::TYPE(fc::log_messages m) : \
    fc::exception() {} \
  const char* TYPE::what() const throw() { return DESCRIPTION; } \
  int32_t TYPE::get_rpc_error_code() const { return ERROR_CODE; }

// the codes here match bitcoine error codes in https://github.com/bitcoin/bitcoin/blob/master/src/rpcprotocol.h#L34
RPC_EXCEPTION_IMPL(rpc_client_not_connected_exception, -9, "The client is not connected to the network")
RPC_EXCEPTION_IMPL(rpc_wallet_unlock_needed_exception, -13, "The wallet's spending key must be unlocked before executing this command")
RPC_EXCEPTION_IMPL(rpc_wallet_passphrase_incorrect_exception, -14, "The wallet passphrase entered was incorrect")

// these error codes don't match anything in bitcoin
RPC_EXCEPTION_IMPL(rpc_wallet_open_needed_exception, -100, "The wallet must be opened before executing this command")

} } // bts::rpc

