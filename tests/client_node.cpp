#include <bts/network/node.hpp>
#include <bts/client/client.hpp>
#include <bts/blockchain/chain_database.hpp>

int main( int argc, char** argv )
{
   try {
     bts::network::node client_node;
     auto c = std::make_shared<bts::client::client>();
     client_node.set_client(c);
     client_node.listen( fc::ip::endpoint::from_string( "0.0.0.0:0" ) );
     client_node.connect_to( fc::ip::endpoint::from_string( "127.0.0.1:3456" ) );

     ilog( "broadcasting block" );
     client_node.broadcast_block( bts::blockchain::full_block() );
     ilog( "sleep for a bit" );
     fc::usleep( fc::seconds(3) );
     ilog( "ready to exit..." );
   } catch ( const fc::exception& e) 
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
   }
   ilog( "exiting" );
   return 0;
}
