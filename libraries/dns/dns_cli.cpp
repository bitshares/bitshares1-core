#include <sstream>
#include <bts/dns/dns_cli.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace dns {

void dns_cli::process_command( const std::string& cmd, const std::string& args )
{
   std::stringstream ss(args);
   if( cmd == "buydomain" )
   {
      
   }
   else if( cmd == "selldomain" )
   {

   }
   else if( cmd == "transferdomain" )
   {

   }
   else if( cmd == "listdomain_auctions" )
   {

   }
   else if( cmd == "lookupdomain" )
   {
   }
   else if( cmd == "updatedomain" )
   {
      std::string name;
      std::string json_value;
      ss >>  name;
      std::getline( ss, json_value );

      // convert arbitrary json value to string..., this validates that it parses
      // properly.
      fc::variant val = fc::json::from_string( json_value );

   }
   else
   {
      cli::process_command( cmd, args );
   }
}


}} //bts::dns
