#pragma once

#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

  struct name_config
  {
     name_config():is_delegate(false){}

     std::string        name;
     public_key_type    owner;
     bool               is_delegate;
  };

  struct asset_config // these are all market-issued assets
  {
     std::string       symbol;
     std::string       name;
     std::string       description;
     uint64_t          precision;
  };
  
  struct genesis_block_config
  {
     fc::time_point_sec                         timestamp;
     std::vector<asset_config>                  market_assets;
     std::vector<name_config>                   names;
     std::vector<std::pair<pts_address,double>> balances;
  };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::name_config, (name)(owner)(is_delegate) )
FC_REFLECT( bts::blockchain::asset_config, (symbol)(name)(description)(precision) )
FC_REFLECT( bts::blockchain::genesis_block_config, (timestamp)(market_assets)(names)(balances) )
