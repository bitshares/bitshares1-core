#pragma once

#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

  struct name_config
  {
     std::string        name;
     public_key_type    owner;
     share_type         delegate_pay_rate = -1;
  };

  struct asset_config // these are all market-issued assets
  {
     std::string       symbol;
     std::string       name;
     std::string       description;
     uint64_t          precision;
     /**
      * min_price is the minimal price of 1 XTS in this asset, e.g. for USD
      * which means 1 XTS = min_price USD, considered the precision of assets
      * for max_price which means the max price of 1 XTS in this asset is max_price USD, 1 XTS = max_price USD
      */
     double            min_price = 0;
     double            max_price = 0;
  };

  struct genesis_block_config
  {
     fc::time_point_sec                         timestamp;
     std::vector<asset_config>                  market_assets;
     std::vector<name_config>                   names;
     std::vector<std::pair<pts_address,double>> balances;
  };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::name_config, (name)(owner)(delegate_pay_rate) )
FC_REFLECT( bts::blockchain::asset_config, (symbol)(name)(description)(precision)(min_price)(max_price) )
FC_REFLECT( bts::blockchain::genesis_block_config, (timestamp)(market_assets)(names)(balances) )
