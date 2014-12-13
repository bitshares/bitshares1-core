#include <bts/wallet/light_wallet.hpp>

int main( int argc, char** argv )
{ 
   try {
      FC_ASSERT( argc > 1, "Usage: ./light_wallet accountname" );
      light_wallet wall;
      wall.connect( "localhost" );
      wall.create( "light_default", "password", "brainseed", "salt" );
      wall.request_register_account( argv[1] );

      // server actions
      std::vector<account_record> wall._rpc.get_registration_requests();
      wall._rpc.approve_register_account( "tester", salt );

      auto balances = wall.balance();
      idump( (balances) );
      
   } 
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e", e.to_detail_string() ) );
   }
   return 0;
}
