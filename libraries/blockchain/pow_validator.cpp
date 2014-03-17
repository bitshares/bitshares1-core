#include <bts/blockchain/pow_validator.hpp>
#include <bts/blockchain/chain_database.hpp>
namespace bts { namespace blockchain {
   bool pow_validator::validate_work( const block_header& header, uint64_t valid_miner_votes )
   {
       return (header.get_difficulty() * valid_miner_votes)  >= _db->current_difficulty();
   }
} } // bts::blockchain 
