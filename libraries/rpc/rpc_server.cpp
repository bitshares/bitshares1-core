#define DEFAULT_LOGGER "rpc"

#include <bts/wallet/exceptions.hpp>
#include <bts/rpc/exceptions.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/utilities/git_revision.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/utilities/padding_ostream.hpp>
#include <bts/net/stcp_socket.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fc/interprocess/file_mapping.hpp>
#include <fc/io/json.hpp>
#include <fc/network/http/server.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/crypto/digest.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/rpc/json_connection.hpp>
#include <fc/thread/thread.hpp>
#include <fc/git_revision.hpp>

#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <iomanip>
#include <limits>
#include <sstream>

#include <bts/rpc_stubs/common_api_rpc_server.hpp>

#include <fc/api.hpp>
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/websocket_api.hpp>

#include <bts/api/reflect_api.hpp>

namespace bts { namespace rpc {

  using namespace client;

  namespace detail
  {
    class rpc_server_impl : public bts::rpc_stubs::common_api_rpc_server
    {
       public:
         rpc_server_config                                 _config;
         bts::client::client*                              _client;
         std::shared_ptr<fc::http::server>                 _httpd;
         std::shared_ptr<fc::tcp_server>                   _tcp_serv;
         std::shared_ptr<fc::tcp_server>                   _stcp_serv;
         std::shared_ptr<fc::http::websocket_server>       _websocket_server;
         fc::future<void>                                  _accept_loop_complete;
         fc::future<void>                                  _encrypted_accept_loop_complete;
         rpc_server*                                       _self;
         fc::shared_ptr<fc::promise<void>>                 _on_quit_promise;
         fc::thread*                                       _thread;
         http_callback_type                                _http_file_callback;
         std::unordered_set<fc::rpc::json_connection_ptr>  _open_json_connections;
         fc::mutex                                         _rpc_mutex; // locked to prevent executing two rpc calls at once

         bool                                              _cache_enabled = true;
         fc::time_point                                    _last_cache_clear_time;
         std::unordered_map<string,string>                 _call_cache;
         std::set<string>                                  _whitelist;

         typedef std::map<std::string, bts::api::method_data> method_map_type;
         method_map_type _method_map;

         /** the map of alias and method name */
         std::map<std::string, std::string>     _alias_map;

         /** the set of connections that have successfully logged in */
         std::unordered_set<fc::rpc::json_connection*> _authenticated_connection_set;

         rpc_server_impl(bts::client::client* client) :
           _client(client),
           _on_quit_promise(new fc::promise<void>("rpc_quit"))
         {}

         void shutdown_rpc_server();

         virtual bts::api::common_api* get_client() const override;
         virtual void verify_json_connection_is_authenticated(fc::rpc::json_connection* json_connection) const override;
         virtual void verify_wallet_is_open() const override;
         virtual void verify_wallet_is_unlocked() const override;
         virtual void verify_connected_to_network() const override;
         virtual void store_method_metadata(const bts::api::method_data& method_metadata)override;

         std::string help(const std::string& command_name) const;

         std::string make_short_description(const bts::api::method_data& method_data, bool show_decription = true) const
         {
           std::string help_string;
           std::stringstream short_description;
           short_description << std::setw(100) << std::left;
           help_string = method_data.name + " ";
           for (const bts::api::parameter_data& parameter : method_data.parameters)
           {
             if (parameter.classification == bts::api::required_positional)
               help_string += std::string("<") + parameter.name + std::string("> ");
             else if (parameter.classification == bts::api::optional_named)
               help_string += std::string("{") + parameter.name + std::string("} ");
             else if (parameter.classification == bts::api::required_positional_hidden)
               continue;
             else
               help_string += std::string("[") + parameter.name + std::string("] ");
           }
           short_description << help_string;
           if (show_decription)
           {
              short_description << "  " << method_data.description << "\n";
           }
           else
           {
              short_description << "\n";
           }
           help_string = short_description.str();
           return help_string;
         }

        void add_content_type_header(const fc::string& path, const fc::http::server::response& s ) {
            static map<string, string> mime_types
            {   {"png", "image/png"},
                {"jpg", "image/jpeg"},
                {"svg", "image/svg+xml"},
                {"css", "text/css"},
                {"html", "text/html"},
                {"js", "application/javascript"},
                {"json", "application/json"},
                {"xml", "application/xml"},
                {"woff", "application/font-woff"},
                {"ttf", "pplication/x-font-ttf"}
            };

            if( path == "/rpc") {
                s.add_header("Content-Type",  "application/json");
                s.add_header("Cache-Control",  "no-cache, no-store, must-revalidate");
                s.add_header("Pragma", "no-cache");
                s.add_header("Expires","0");
                return;
            }

            auto pos = path.rfind('.');
            if(pos != std::string::npos) {
                auto extension = path.substr(pos + 1, std::string::npos);
                auto mime_type = mime_types.find(extension);
                if(mime_type != mime_types.end())
                    s.add_header("Content-Type", mime_type->second);
            }
        }

         void handle_request( const fc::http::request& r, const fc::http::server::response& s )
         {
             fc::time_point begin_time = fc::time_point::now();
             // WARNING: logging RPC calls can capture passwords and private keys
             //fc_ilog( fc::logger::get("rpc"), "Started ${path} ${method} at ${time}", ("path",r.path)("method",r.method)("time",begin_time));
            // dlog( "${r}", ("r",r.path) );
             fc::http::reply::status_code status = fc::http::reply::OK;

             s.add_header( "Connection", "close" );

             fc::oexception internal_server_error;
             bool invalid_request_error = false;

             try {
                if( _config.rpc_user.size() )
                {
              //     dlog( "CHECKING AUTH" );
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

               //    ilog( "\n\n\n  username: ${user}  password ${password}", ("path",r.path)("user",username)("password", password));
                   if( _config.rpc_user     != username ||
                       _config.rpc_password != password )
                   {
                      //fc_ilog( fc::logger::get("rpc"), "Unauthorized ${path}, username: ${user}", ("path",r.path)("user",username));
                   // WARNING: logging RPC calls can capture passwords and private keys
                //      elog( "\n\n\n Unauthorized ${path}, username: ${user}  password ${password}", ("path",r.path)("user",username)("password", password));
                      s.add_header( "WWW-Authenticate", "Basic realm=\"bts wallet\"" );
                      std::string message = "Unauthorized";

                      s.set_status( fc::http::reply::NotAuthorized );
                      s.set_length( message.size() );
                      s.write( message.c_str(), message.size() );
                      //elog( "UNAUTHORIZED ${r} ${cfg}  ${user} : ${provided}", ("r",r.path)("cfg",_config)("provided",password)("user",username)  );
                      return;
                   }
                   else
                   {
                    //wlog( "NOT CHECKING AUTH" );
                   }
                }
                //ilog( "rpc_user.size: ${config}", ("config",_config.rpc_user.size() ) );

               // wlog( "providing data anyway???" );

                fc::string path = r.path;
                auto pos = path.find( '?' );
                if( pos != std::string::npos ) path.resize(pos);

                pos = path.find( ".." );
                FC_ASSERT( pos == std::string::npos );

                if( path == "/" ) path = "/index.html";
                 add_content_type_header(path,s);

                auto filename = _config.htdocs / path.substr(1,std::string::npos);
                if( r.path == fc::path("/rpc")
                        || fc::path(r.path).parent_path() == fc::path("/rpc"))
                {
                   // WARNING: logging RPC calls can capture passwords and private keys
                  //  dlog( "RPC ${r}", ("r",r.path) );
                    status = handle_http_rpc( r, s );
                }
                else if( fc::path(r.path).parent_path() == fc::path("/safebot") )
                {
                   // WARNING: logging RPC calls can capture passwords and private keys
                  //  dlog( "RPC ${r}", ("r",r.path) );
                    status = handle_http_rpc( r, s );
                }
                else if( _http_file_callback )
                {
                   _http_file_callback( path, s );
                }
                else if( fc::exists( filename ) )
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
             catch ( const fc::canceled_exception& )
             {
                    throw;
             }
             catch ( const fc::exception& e )
             {
                    internal_server_error = e;
             }
             catch ( ... )
             {
                    invalid_request_error = true;
             }

             if (internal_server_error)
             {
                    std::string message = "Internal Server Error\n";
                    message += internal_server_error->to_detail_string();
                    fc_ilog( fc::logger::get("rpc"), "Internal Server Error ${path} - ${msg}", ("path",r.path)("msg",message));
                    elog("Internal Server Error ${path} - ${msg}", ("path",r.path)("msg",message));
                    s.set_length( message.size() );
                    s.set_status( fc::http::reply::InternalServerError );
                    s.write( message.c_str(), message.size() );
                    elog( "${e}", ("e", internal_server_error->to_detail_string() ) );
                    status = fc::http::reply::InternalServerError;
             }
             else if (invalid_request_error)
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

         void check_whitelist( const std::string& method ) {
             if ( _whitelist.empty() ) { return; }
             FC_ASSERT( _whitelist.find( method ) != _whitelist.end() );
         }

         /** Assures that if the request path is either /req or
          *  /safebot/whatever, or of the form /req/something where
          *  something == method.
          *  throws an exception otherwise.
          */
         static void validate_request_path(const fc::path& req, const string& method,
                                           fc::variants& parameters)
         {
             static fc::path prefix("/rpc");
             if ( req.parent_path() == fc::path("/safebot") || req == prefix )
             {
                 return;
             }
             if ( method == "batch" )
             {
                 FC_ASSERT( parameters.size() > 0 && parameters[0].is_string() && req == prefix / parameters[0].as_string() );
             }
             else
             {
                 FC_ASSERT( req == prefix / method );
             }
         }

         static string send_and_log_reply( const fc::http::server::response& s,
                                           const fc::http::reply::status_code status,
                                           const fc::path& path,
                                           const string& method,
                                           const fc::mutable_variant_object& json)
         {
             s.set_status( status );
             auto reply = fc::json::to_string( json );
             s.set_length( reply.size() );
             s.write( reply.c_str(), reply.size() );
             auto reply_log = reply.size() > 253 ? reply.substr(0,253) + ".." :  reply;
             fc_ilog( fc::logger::get("rpc"), "Result ${path} ${method}: ${reply}", ("path",path)("method",method)("reply",reply_log));
             return reply;
         }

         fc::http::reply::status_code handle_http_rpc(const fc::http::request& r, const fc::http::server::response& s )
         {
                fc::http::reply::status_code status = fc::http::reply::OK;
                std::string str(r.body.data(),r.body.size());
                //wlog( "RPC: ${r}", ("r",str) );
                fc::string method_name;

                fc::optional<std::string> invalid_rpc_request_message;

                try {
                   auto rpc_call = fc::json::from_string( str ).get_object();
                   method_name = rpc_call["method"].as_string();
                   check_whitelist( method_name );
                   auto params = rpc_call["params"].get_array();
                   validate_request_path( r.path, method_name, params );

                   auto request_key = method_name + "=" + fc::json::to_string(rpc_call["params"]);


                   if( fc::time_point(bts::blockchain::now()) - _last_cache_clear_time  > fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC) )
                   {
                      _last_cache_clear_time = bts::blockchain::now();
                      _call_cache.clear();
                   }
                   else if( _cache_enabled )
                   {
                      auto cache_itr = _call_cache.find(request_key);
                      if( cache_itr != _call_cache.end() )
                      {
                         status = fc::http::reply::OK;
                         fc::mutable_variant_object json( fc::json::from_string( cache_itr->second ) );
                         json["id"] =  rpc_call["id"];
                         send_and_log_reply( s, status, r.path, method_name, json );
                         return status;
                      }
                   }


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
                      //   auto reply = fc::json::to_string( result );
                         status = fc::http::reply::OK;
                      }
                      catch ( const fc::canceled_exception& )
                      {
                          throw;
                      }
                      catch ( const fc::exception& e )
                      {
                          status = fc::http::reply::InternalServerError;
                          result["error"] = fc::mutable_variant_object("message",e.to_string())( "detail",e.to_detail_string() )("code",e.code());
                      }
                      //ilog( "${e}", ("e",result) );
                      auto reply = send_and_log_reply( s, status, r.path, method_name, result );
                      if( _method_map[method_name].cached )
                         _call_cache[request_key] = std::move( reply );

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
                catch ( const fc::canceled_exception& )
                {
                    throw;
                }
                catch ( const fc::exception& e )
                {
                    invalid_rpc_request_message = e.to_detail_string();
                }
                catch (const std::exception& e)
                {
                    invalid_rpc_request_message = e.what();
                }
                catch (...)
                {
                    invalid_rpc_request_message = "...";
                }

                if (invalid_rpc_request_message)
                {
                    fc_ilog( fc::logger::get("rpc"), "Invalid RPC Request ${path} ${method}: ${e}", ("path",r.path)("method",method_name)("e", *invalid_rpc_request_message));
                    elog( "Invalid RPC Request ${path} ${method}: ${e}", ("path",r.path)("method",method_name)("e",*invalid_rpc_request_message));
                    std::string message = "Invalid RPC Request\n";
                    message += *invalid_rpc_request_message;
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
                _tcp_serv->accept( *sock );
              }
              catch (const fc::canceled_exception&)
              {
                throw;
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
              auto receipt = _open_json_connections.insert(json_con);

              json_con->exec().on_complete([this,receipt,sock](fc::exception_ptr e){
                  ilog("json_con exited");
                  sock->close();
                  _open_json_connections.erase(receipt.first);
                  if( e )
                    elog("Connection exited with error: ${error}", ("error", e->what()));
              });
           }
         }

         void encrypted_accept_loop(const bts::blockchain::private_key_type& server_key)
         {
           while( !_encrypted_accept_loop_complete.canceled() )
           {
              net::stcp_socket_ptr sock = std::make_shared<net::stcp_socket>();
              try
              {
                _stcp_serv->accept( sock->get_socket() );
                sock->accept();
                auto signature = server_key.sign_compact(fc::digest(sock->get_shared_secret()));
                auto padded_signature = fc::raw::pack(signature);
                while( padded_signature.size() % 16 )
                   padded_signature.push_back(0);
                sock->write(padded_signature.data(), padded_signature.size());
              }
              catch (const fc::canceled_exception&)
              {
                throw;
              }
              catch ( const fc::exception& e )
              {
                elog( "fatal: error opening socket for rpc connection: ${e}", ("e", e.to_detail_string() ) );
                continue;
              }

              auto buf_istream = std::make_shared<fc::buffered_istream>( sock );
              auto buf_ostream = std::make_shared<utilities::padding_ostream<>>( sock );

              auto json_con = std::make_shared<fc::rpc::json_connection>( std::move(buf_istream),
                                                                          std::move(buf_ostream) );
              register_methods( json_con );
              auto receipt = _open_json_connections.insert(json_con);

              json_con->exec().on_complete([this,receipt,sock](fc::exception_ptr e){
                  ilog("json_con exited");
                  sock->close();
                  _open_json_connections.erase(receipt.first);
                  if( e )
                    elog("Connection exited with error: ${error}", ("error", e->what()));
              });
           }
         }

         void register_methods( fc::rpc::json_connection_ptr con )
         {
            ilog( "login!" );
            fc::rpc::json_connection* capture_con = con.get();

            // the login method is a special case that is only used for raw json connections
            // (not for the CLI or HTTP(s) json rpc)
            try
            {
              check_whitelist( "login" );
              con->add_method("login", boost::bind(&rpc_server_impl::login, this, capture_con, _1));
            }
            catch ( fc::assert_exception skip ) {}
            for (const method_map_type::value_type& method : _method_map)
            {
              if (method.second.method)
              {
                // old method using old registration system
                auto bind_method = boost::bind(&rpc_server_impl::dispatch_method_from_json_connection,
                                                          this, capture_con, method.second, _1);
                try
                {
                  check_whitelist( method.first );
                  con->add_method(method.first, bind_method);
                  for ( const auto& alias : method.second.aliases )
                    con->add_method(alias, bind_method);
                }
                catch ( fc::assert_exception skip ) {}
              }
            }

            register_common_api_methods(con);
            // FIXME: this assumes that reg_com_api_met only registers methods
            // that are also in _method_map. That sucks.
            for (const method_map_type::value_type& method : _method_map)
            {
              try
              {
                check_whitelist( method.first );
              }
              catch ( fc::assert_exception remove )
              {
                con->remove_method( method.first );
                for ( const auto& alias : method.second.aliases )
                {
                  con->remove_method( alias );
                }
              }
            }
         } // register methods

        fc::variant dispatch_method_from_json_connection(fc::rpc::json_connection* con,
                                                         const bts::api::method_data& method_data,
                                                         const fc::variants& arguments)
        {
          // ilog( "arguments: ${params}", ("params",arguments) );
          if ((method_data.prerequisites & bts::api::json_authenticated) &&
              _authenticated_connection_set.find(con) == _authenticated_connection_set.end())
            FC_THROW_EXCEPTION( login_required, "not logged in");
          return dispatch_authenticated_method(method_data, arguments);
        }

        fc::variant dispatch_authenticated_method(const bts::api::method_data& method_data,
                                                  const fc::variants& arguments_from_caller)
        {
          fc::scoped_lock<fc::mutex> lock(_rpc_mutex);

          if (!method_data.method)
          {
            // then this is a method using our new generated code
            return direct_invoke_positional_method(method_data.name, arguments_from_caller);
          }
          //ilog("method: ${method_name}, arguments: ${params}", ("method_name", method_data.name)("params",arguments_from_caller));
          if (method_data.prerequisites & bts::api::wallet_open)
            verify_wallet_is_open();
          if (method_data.prerequisites & bts::api::wallet_unlocked)
            verify_wallet_is_unlocked();
          if (method_data.prerequisites & bts::api::connected_to_network)
            verify_connected_to_network();

          fc::variants modified_positional_arguments;
          fc::mutable_variant_object modified_named_arguments;

          bool has_named_parameters = false;
          unsigned next_argument_index = 0;
          for (const bts::api::parameter_data& parameter : method_data.parameters)
          {
            if (parameter.classification == bts::api::required_positional
                || parameter.classification == bts::api::required_positional_hidden)
            {
              if (arguments_from_caller.size() < next_argument_index + 1)
                FC_THROW_EXCEPTION(missing_parameter, "missing required parameter ${parameter_name}", ("parameter_name", parameter.name));
              modified_positional_arguments.push_back(arguments_from_caller[next_argument_index++]);
            }
            else if (parameter.classification == bts::api::optional_positional)
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
            else if (parameter.classification == bts::api::optional_named)
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
            FC_THROW_EXCEPTION( unknown_method, "Invalid command ${command}", ("command", method_name));
          return dispatch_authenticated_method(_method_map[iter->second], arguments);
        }

        fc::variant login( fc::rpc::json_connection* json_connection, const fc::variants& params );

        void add_whitelist_entry( const std::string& entry ) {
          if ( entry == "login" )
          {
            _whitelist.insert( entry );
            return;
          }
          auto method = _method_map.find( entry );
          if ( method == _method_map.end() )
          {
            const auto& alias = _alias_map.find( entry );
            if ( alias == _alias_map.end() )
            {
              wlog("Whitelist contains unknown method ${method}", ("method", entry));
              return;
            }
            add_whitelist_entry( alias->second );
            return;
          }
          _whitelist.insert( entry );
          for ( const auto& alias : method->second.aliases )
          {
            _whitelist.insert( alias );
          }
        }

        void add_whitelist(const std::set<std::string>& new_entries) {
          for (const auto& entry : new_entries)
          {
            add_whitelist_entry( entry );
          }
        }
    };

    bts::api::common_api* rpc_server_impl::get_client() const
    {
      return _client;
    }
    void rpc_server_impl::verify_json_connection_is_authenticated(fc::rpc::json_connection* json_connection) const
    {
      if (json_connection &&
          _authenticated_connection_set.find(json_connection) == _authenticated_connection_set.end())
        FC_THROW("The RPC connection must be logged in before executing this command");
    }
    void rpc_server_impl::verify_wallet_is_open() const
    {
      if (!_client->get_wallet()->is_open())
        throw rpc_wallet_open_needed_exception(FC_LOG_MESSAGE(error, "The wallet must be open before executing this command"));
    }
    void rpc_server_impl::verify_wallet_is_unlocked() const
    {
      if (_client->get_wallet()->is_locked())
        throw rpc_wallet_unlock_needed_exception(FC_LOG_MESSAGE(error, "The wallet's spending key must be unlocked before executing this command"));
    }
    void rpc_server_impl::verify_connected_to_network() const
    {
      if (!_client->is_connected())
        throw rpc_client_not_connected_exception(FC_LOG_MESSAGE(error, "The client must be connected to the network to execute this command"));
    }
    void rpc_server_impl::store_method_metadata(const bts::api::method_data& method_metadata)
    {
      _self->register_method(method_metadata);
    }

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
      FC_ASSERT( params[0].as_string() == _config.rpc_user );
      FC_ASSERT( params[1].as_string() == _config.rpc_password );
      _authenticated_connection_set.insert( json_connection );
      return fc::variant( true );
    }

    std::string rpc_server_impl::help(const std::string& command_name) const
    {
      std::string help_string;
      if (command_name.empty()) //if no arguments, display list of commands with short description
      {
        for (const method_map_type::value_type& method_data_pair : _method_map)
          if (method_data_pair.second.name[0] != '_' && method_data_pair.second.name.substr(0, 8) != "bitcoin_") // hide undocumented commands
            help_string += make_short_description(method_data_pair.second, false);
      }
      else
      { //display detailed description of requested command
        auto alias_iter = _alias_map.find(command_name);
        if (alias_iter != _alias_map.end())
        {
          auto method_iter = _method_map.find(alias_iter->second);
          FC_ASSERT(method_iter != _method_map.end(), "internal error, mismatch between _method_map and _alias_map");
          const bts::api::method_data& method_data = method_iter->second;
          help_string += "Usage:\n";
          help_string += make_short_description(method_data);
          help_string += method_data.detailed_description;
          if (method_data.aliases.size() > 0)
            help_string += "\naliases: " + boost::join(method_data.aliases, ", ");
        }
        else
        {
          // no exact matches for the command they requested.
          // If they give us a prefix, give them the list of commands that start
          // with that prefix (i.e. "help wallet" will return wallet_open, wallet_close, &c)
          std::set<std::string> matching_commands;
          for (auto method_iter = _method_map.lower_bound(command_name);
               method_iter != _method_map.end() && method_iter->first.compare(0, command_name.size(), command_name) == 0;
               ++method_iter)
            matching_commands.insert(method_iter->first);
          // If they give us an alias (or its prefix), give them the list of real command names, eliminating duplication
          for (auto alias_itr = _alias_map.lower_bound(command_name);
               alias_itr != _alias_map.end() && alias_itr->first.compare(0, command_name.size(), command_name) == 0;
               ++alias_itr)
            matching_commands.insert(alias_itr->second);
          for (auto command : matching_commands)
          {
            auto method_iter = _method_map.find(command);
            FC_ASSERT(method_iter != _method_map.end(), "internal error, mismatch between _method_map and _alias_map");
            help_string += make_short_description(method_iter->second);
          }
          if (help_string.empty())
            throw rpc_misc_error_exception(FC_LOG_MESSAGE( error, "No help available for command \"${command}\"", ("command", command_name)));
        }
      }

      return boost::algorithm::trim_copy(help_string);
    }

  } // detail


  rpc_server::rpc_server(bts::client::client* client) :
    my(new detail::rpc_server_impl(client))
  {
    my->_thread = &fc::thread::current();
    my->_self = this;
    try {
       my->register_common_api_method_metadata();
    }FC_RETHROW_EXCEPTIONS( warn, "register common api" )
  }

  rpc_server::~rpc_server()
  {
    try
    {
      shutdown_rpc_server();
      wait_till_rpc_server_shutdown();
      // just to be safe, destroy the  servers inside this try/catch block in case they throw
      my->_tcp_serv.reset();
      my->_httpd.reset();
    }
    catch ( const fc::exception& e )
    {
      wlog( "unhandled exception thrown in destructor.\n${e}", ("e", e.to_detail_string() ) );
    }
  }

  bool rpc_server::configure_rpc( const rpc_server_config& cfg )
  {
    if (!cfg.is_valid())
      return false;
    my->_cache_enabled = cfg.enable_cache;

    try
    {
      my->_config = cfg;
      my->_tcp_serv = std::make_shared<fc::tcp_server>();
      int attempts = 0;
      bool success = false;

      while (!success) {
        try
        {
          my->_tcp_serv->listen( cfg.rpc_endpoint );
          success = true;
        }
        catch (fc::exception& e)
        {
          FC_ASSERT(++attempts < 30, "Unable to bind RPC port; refusing to continue.");
          ulog("Failed to bind RPC port ${endpoint}; waiting 10 seconds and retrying (attempt ${attempt}/30)",
               ("endpoint", cfg.rpc_endpoint)("attempt", attempts));
          elog("Failed to bind RPC port ${endpoint} with error ${e}", ("endpoint", cfg.rpc_endpoint)("e", e.to_detail_string()));
        }
        if (!success)
          fc::usleep(fc::seconds(10));
      }

      ilog( "listening for json rpc connections on port ${port}", ("port",my->_tcp_serv->get_port()) );

      my->add_whitelist( cfg.whitelist );

      my->_accept_loop_complete = fc::async( [=]{ my->accept_loop(); }, "rpc_server accept_loop" );

      return true;
    } FC_RETHROW_EXCEPTIONS( warn, "attempting to configure rpc server ${port}", ("port",cfg.rpc_endpoint)("config",cfg) );
  }

  bool rpc_server::configure_websockets( const rpc_server_config& cfg )
  {
    if (!cfg.is_valid())
      return false;
    my->_cache_enabled = cfg.enable_cache;

    try
    {
      my->_websocket_server = std::make_shared<fc::http::websocket_server>();

      my->_websocket_server->on_connection([&]( const fc::http::websocket_connection_ptr& c ){
               auto wsc = std::make_shared<fc::rpc::websocket_api_connection>(*c);
               auto login = std::make_shared<bts::api::login_api>( cfg.users, my->_client );
               wsc->register_api(fc::api<bts::api::login_api>(login));
               c->set_session_data( wsc );
          });

      my->_websocket_server->listen( cfg.websocket_endpoint.port() );
      my->_websocket_server->start_accept();
      ilog( "listening for websocket json rpc connections on port ${port}", ("port",cfg.rpc_endpoint.port())  );

      return true;
    } FC_RETHROW_EXCEPTIONS( warn, "attempting to configure rpc server ${port}", ("port",cfg.rpc_endpoint)("config",cfg) );
  }

  bool rpc_server::configure_http(const rpc_server_config& cfg)
  {
    if(!cfg.is_valid())
      return false;

    my->_cache_enabled = cfg.enable_cache;

    try
    {
      my->_config = cfg;
      auto m = my.get();
      my->_httpd = std::make_shared<fc::http::server>();
      int attempts = 0;
      bool success = false;

      while (!success) {
        try
        {
          my->_httpd->listen( cfg.httpd_endpoint );
          success = true;
        }
        catch (fc::exception& e)
        {
          FC_ASSERT(++attempts < 30, "Unable to bind HTTPD port; refusing to continue.");
          ulog("Failed to bind HTTPD port ${endpoint}; waiting 10 seconds and retrying (attempt ${attempt}/30)",
               ("endpoint", cfg.httpd_endpoint)("attempt", attempts));
          elog("Failed to bind HTTPD port ${endpoint} with error ${e}", ("endpoint", cfg.rpc_endpoint)("e", e.to_detail_string()));
        }
        if (!success)
          fc::usleep(fc::seconds(10));
      }

      my->add_whitelist( cfg.whitelist );

      my->_httpd->on_request([m](const fc::http::request& r, const fc::http::server::response& s){ m->handle_request(r, s); });
      return true;
    } FC_RETHROW_EXCEPTIONS(warn, "attempting to configure rpc server ${port}", ("port", cfg.rpc_endpoint)("config", cfg));
  }

  bool rpc_server::configure_encrypted_rpc(const rpc_server_config& cfg)
  {
    if (!cfg.is_valid())
      return false;
    my->_cache_enabled = cfg.enable_cache;
    if(cfg.encrypted_rpc_wif_key.empty())
    {
       std::cerr << ("No WIF");
       return false;
    }
    auto server_key = utilities::wif_to_key(cfg.encrypted_rpc_wif_key);
    if(!server_key.valid())
    {
       std::cerr << ("Invalid WIF");
       return false;
    }

    try
    {
       my->_config = cfg;
       my->_stcp_serv = std::make_shared<fc::tcp_server>();
       int attempts = 0;
       bool success = false;

       while (!success) {
          try
          {
             my->_stcp_serv->listen( cfg.encrypted_rpc_endpoint );
             success = true;
          }
          catch (fc::exception& e)
          {
             FC_ASSERT(++attempts < 30, "Unable to bind encrypted RPC port; refusing to continue.");
             ulog("Failed to bind encrypted RPC port ${endpoint}; waiting 10 seconds and retrying (attempt ${attempt}/30)",
                  ("endpoint", cfg.encrypted_rpc_endpoint)("attempt", attempts));
             elog("Failed to bind encrypted RPC port ${endpoint} with error ${e}", ("endpoint", cfg.encrypted_rpc_endpoint)("e", e.to_detail_string()));
          }
          if (!success)
             fc::usleep(fc::seconds(10));
       }

       ilog( "listening for encrypted json rpc connections on port ${port}", ("port",my->_stcp_serv->get_port()) );

       my->add_whitelist( cfg.whitelist );

       my->_encrypted_accept_loop_complete = fc::async( [=]{ my->encrypted_accept_loop(*server_key); }, "rpc_server accept_loop" );

       return true;
    } FC_RETHROW_EXCEPTIONS( warn, "attempting to configure rpc server ${port}", ("port",cfg.encrypted_rpc_endpoint)("config",cfg) );

  }

  fc::variant rpc_server::direct_invoke_method(const std::string& method_name, const fc::variants& arguments)
  {
    return my->direct_invoke_method(method_name, arguments);
  }

  const bts::api::method_data& rpc_server::get_method_data(const std::string& method_name)
  {
    auto iter = my->_alias_map.find(method_name);
    if (iter != my->_alias_map.end())
      return my->_method_map[iter->second];
    FC_THROW_EXCEPTION(unknown_method, "Method \"${name}\" not found", ("name", method_name));
  }
  void rpc_server::set_http_file_callback(  const http_callback_type& callback )
  {
     my->_http_file_callback = callback;
  }

  std::vector<bts::api::method_data> rpc_server::get_all_method_data() const
  {
    std::vector<bts::api::method_data> result;
    for (const detail::rpc_server_impl::method_map_type::value_type& value : my->_method_map)
      result.emplace_back(value.second);
    return result;
  }

  void rpc_server::register_method(bts::api::method_data data)
  {
    FC_ASSERT(my->_alias_map.find(data.name) == my->_alias_map.end(), "attempting to register an exsiting method name ${m}", ("m", data.name));
    my->_alias_map[data.name] = data.name;
    for ( auto alias : data.aliases )
    {
        FC_ASSERT(my->_alias_map.find(alias) == my->_alias_map.end(), "attempting to register an exsiting method name ${m}", ("m", alias));
        my->_alias_map[alias] = data.name;
    }
    my->_method_map.insert(detail::rpc_server_impl::method_map_type::value_type(data.name, data));
  }


  void rpc_server::wait_till_rpc_server_shutdown()
  {
    // wait until a quit has been signalled
    try
    {
      my->_on_quit_promise->wait();
    }
    catch (const fc::canceled_exception&)
    {
    }
    // if we were running a TCP server, also wait for it to shut down
    if (my->_tcp_serv && my->_accept_loop_complete.valid())
    {
      try
      {
        my->_accept_loop_complete.wait();
      }
      catch (const fc::canceled_exception&)
      {
      }
    }
  }

  void rpc_server::shutdown_rpc_server()
  {
    // shutdown the server.  add a little delay to give the response to the "stop" method call a chance
    // to make it to the caller
    // my->_thread->async([=]() { fc::usleep(fc::milliseconds(10)); close(); });
    // Because we never waited on the above call we would crash... when rpc_server is
    // deleted before it can execute.
    if( my->_on_quit_promise )
       my->_on_quit_promise->set_value();
    if (my->_tcp_serv)
      my->_tcp_serv->close();
    if( my->_accept_loop_complete.valid() && !my->_accept_loop_complete.ready())
      my->_accept_loop_complete.cancel(__FUNCTION__);
  }

  std::string rpc_server::help(const std::string& command_name) const
  {
    return my->help(command_name);
  }

  method_map_type rpc_server::meta_help()const
  {
     return my->_method_map;
  }

  fc::optional<fc::ip::endpoint> rpc_server::get_rpc_endpoint() const
  {
    if (my->_tcp_serv)
      return my->_tcp_serv->get_local_endpoint();
    return fc::optional<fc::ip::endpoint>();
  }

  fc::optional<fc::ip::endpoint> rpc_server::get_httpd_endpoint() const
  {
    if (my->_httpd)
      return my->_httpd->get_local_endpoint();
    return fc::optional<fc::ip::endpoint>();
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
