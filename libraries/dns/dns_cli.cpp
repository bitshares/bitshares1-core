#include <bts/dns/dns_cli.hpp>

#include <iostream>

namespace bts { namespace dns {

void dns_cli::process_command( const std::string& cmd, const std::string& args )
{
   std::stringstream ss(args);
   if( cmd == "buy_domain" )
   {
       const dns_wallet_ptr wall = std::dynamic_pointer_cast<dns_wallet>(get_client()->get_wallet());
       const dns_db_ptr db = std::dynamic_pointer_cast<dns_db>(get_client()->get_chain());
       
       std::string name = "BITSHARES";
       asset bid = asset(uint64_t(1));
       signed_transactions tx_pool;

       printf("Unlocking wallet\n");
       wall->unlock_wallet( "AAAAAAAAA" );

       printf("Bidding on name\n");
       auto tx = wall->bid_on_domain(name, bid, tx_pool, *db);

      get_client()->broadcast_transaction( tx );      
   }
   else if( cmd == "sell_domain" )
   {

   }
   else if( cmd == "transfer_domain" )
   {

   }
   else if( cmd == "list_active_actions" )
   {

   }
   else if( cmd == "lookup_value" )
   {
   }
   else if( cmd == "update_value" )
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
       printf("Falling back to original CLI cmd\n");
      cli::process_command( cmd, args );
   }
}

void dns_cli::list_transactions( uint32_t count)
{
}

void dns_cli::get_balance( uint32_t min_conf, uint16_t unit)
{
}

}} //bts::dns
