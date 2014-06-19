#pragma once
#include <bts/db/level_map.hpp>

struct registration_request
{
   std::string                       account_name;
   bts::blockchain::public_key_type  key;
   std::string                       email;
};

struct registration_record
{
   registration_request              request;
   fc::sha256                        code;
   bool                              validated;
   bool                              registered;
   std::string                       referred_by;
};

FC_REFLECT( registration_request, (account_name)(key)(email) );
FC_REFLECT( registration_record, (request)(code)(validated)(registered)(referred_by) );


class faucet 
{
   public:
     faucet()
     {
     }

     void open( const fc::path& data_dir )
     {
        _registration_db.open( data_dir / "registration_db", true );
        _code_index.open( data_dir / "code_index_db", true );
     }

     void listen( uint16_t port )
     {
        _httpd = std::make_shared<fc::http::server>();

        try
        {
          _httpd->listen(port);
        }
        catch (fc::exception& e)
        {
          if (cfg.httpd_endpoint.port() != 0)
          {
            wlog("unable to listen on endpoint ${endpoint}", ("endpoint", cfg.httpd_endpoint));
            fc::ip::endpoint any_port_endpoint = cfg.httpd_endpoint;
            any_port_endpoint.set_port(0);
            try
            {
              _httpd->listen(any_port_endpoint);
            }
            catch (fc::exception& e)
            {
              wlog("unable to listen on endpoint ${endpoint}", ("endpoint", any_port_endpoint));
              FC_RETHROW_EXCEPTION(e, error, "unable to listen for HTTP JSON RPC connections on endpoint ${firstchoice} or our fallback ${secondchoice}", 
                                   ("firstchoice", cfg.httpd_endpoint)("secondchoice", any_port_endpoint));
            }
          }
          else
            FC_RETHROW_EXCEPTION(e, error, 
                                 "unable to listen for HTTP JSON RPC connections on endpoint ${endpoint}", 
                                 ("endpoint", cfg.httpd_endpoint));
        }
        _httpd->on_request( [=]( const fc::http::request& r, const fc::http::server::response& s ){ handle_request( r, s ); } );
     }

     void handle_register( const registration_request& request, const fc::http::server::response& s )
     {
         // check unique
         auto reg_itr = _registration_db.find( request.email );
         FC_ASSERT( NOT reg_itr.valid() );

         auto lower_email = fc::to_lower( request.email );
         auto email_hash  = std::string(fc::md5::hash( request.email ));

         // check gravatar
         // should throw 404 if not found...
         fc::http::get( "http://www.gravatar.com/avatar/" + email_hash + "?d=404");

         // generate code

         // send email 
         
     }

     void handle_confirm( const string& code,  const fc::http::server::response& s )
     {
        auto itr = _code_index.find(code);
        if( itr.valid() )
        {
           auto reg_record = _registration_db.fetch( itr.second() ):
           FC_ASSERT( !reg_record.validated );

           // client add contact and then register it

        }
        else
        {
           FC_ASSERT( !"Invalid Code" );
        }
     }

     void handle_request( const fc::http::request& r, const fc::http::server::response& s )
     {
        try {
            s.add_header( "Connection", "close" );
            if( path == "/register" ) 
            {
                std::string str(r.body.data(),r.body.size());
                auto req = fc::json::from_string(str).as<registration_request>();
                handle_register( req );
            }
            else if( path.parent_path() == "/confirm" )
            {
                handle_confirm( path.filename().generic_string() );
            }
            else
            {
                 std::string not_found = "not found";
                 s.set_status( fc::http::reply::NotFound );
                 s.set_length( not_found.size() );
                 s.write( not_found.c_str(), not_found.size() );
            }
        } 
        catch ( const fc::exception& e )
        {
             std::string message = "Invalid RPC Request\n";
             s.set_length( message.size() );
             s.set_status( fc::http::reply::BadRequest );
             s.write( message.c_str(), message.size() );
             status = fc::http::reply::BadRequest;
        }
     }

     std::shared_ptr<fc::http::server>                     _httpd;
     bts::db::level_map<std::string, registration_record>  _registration_db;
     bts::db::level_map<std::string, std::string>          _code_index;


};
