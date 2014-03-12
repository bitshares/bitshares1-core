#include <bts/blockchain/pow_validator.hpp>
#include <bts/blockchain/chain_database.hpp>

namespace bts { namespace blockchain {

   fc::time_point sim_pow_validator::get_time()const
   {
      return fc::time_point(_db->get_head_block().timestamp) + fc::seconds(60);
   }

} } // bts::blockchain
 
