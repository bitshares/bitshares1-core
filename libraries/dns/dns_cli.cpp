#include <sstream>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>


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
      bool pretty = true;
      auto value = fc::raw::unpack<fc::variant>( output.value );
      fc::json::to_pretty_string( value );
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

      claim_domain_output out;

      // convert variant to binary serialized version.. should be smaller.
      out.value = fc::raw::pack( val );
   }
   else
   {
      cli::process_command( cmd, args );
   }
}
