#include <bts/blockchain/pow_validator.hpp>
#include <bts/blockchain/chain_database.hpp>
namespace bts { namespace blockchain {
   bool pow_validator::validate_work( const block_header& header )
   {
       return header.get_difficulty() >= _db->current_difficulty();
   }
} } // bts::blockchain 
