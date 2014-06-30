#include <bts/network/node.hpp>
#include <bts/client/client.hpp>
#include <bts/blockchain/chain_database.hpp>

int main( int argc, char** argv )
{
   try {
     auto c = std::make_shared<bts::client::client>();
     bts::network::node server_node;
     server_node.set_client(c);
     server_node.listen( fc::ip::endpoint::from_string( "0.0.0.0:3456" ) );
     ilog( "ready for connections" );
     fc::usleep( fc::seconds(10000 ) );
   } catch ( const fc::exception& e) 
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
   }

}
