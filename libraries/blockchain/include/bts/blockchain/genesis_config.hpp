#pragma once

#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

  struct name_config
  {
     std::string        name;
     public_key_type    owner;
     uint8_t            delegate_pay_rate = -1;
  };

  struct asset_config // these are all market-issued assets
  {
     std::string    symbol;
     std::string    name;
     std::string    description;
     uint64_t       precision = 1;
  };

  enum balance_type
  {
      genesis = 0,
      bts_exodus_sharedrop = 1
  };

  struct special_balance_config
  {
     std::string    raw_address;
     share_type     balance;
     balance_type   type;
  };

  struct genesis_block_config
  {
     fc::time_point_sec                             timestamp;
     std::vector<asset_config>                      market_assets;
     std::vector<name_config>                       names;
     std::vector<std::pair<pts_address,share_type>> balances;
     std::vector<special_balance_config>            bts_sharedrop;
  };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::name_config, (name)(owner)(delegate_pay_rate) )
FC_REFLECT( bts::blockchain::asset_config, (symbol)(name)(description)(precision) )
FC_REFLECT_ENUM( bts::blockchain::balance_type, (genesis)(bts_exodus_sharedrop) )
FC_REFLECT( bts::blockchain::special_balance_config, (raw_address)(balance)(type) )
FC_REFLECT( bts::blockchain::genesis_block_config, (timestamp)(market_assets)(names)(balances) )
