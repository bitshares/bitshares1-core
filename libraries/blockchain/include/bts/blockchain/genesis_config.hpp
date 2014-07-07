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

  struct genesis_block_config
  {
     fc::time_point_sec                             timestamp;
     std::vector< name_config >                     names;
     std::vector<std::pair<pts_address, double>>    balances;
#if BTS_BLOCKCHAIN_VERSION > 104
#warning [HARDFORK] Remove below deprecated members
#else
     genesis_block_config():supply(0),precision(1000000) {}
     int64_t                                                        supply;
     int64_t                                                        precision;
     std::string                                                    base_symbol;
     std::string                                                    base_name;
     std::string                                                    base_description;
#endif
  };

} } // bts::blockchain

#if BTS_BLOCKCHAIN_VERSION > 104
FC_REFLECT( bts::blockchain::name_config, (name)(owner)(is_delegate) )
FC_REFLECT( bts::blockchain::genesis_block_config, (timestamp)(names)(balances) )
#warning [HARDFORK] Remove below deprecated members
#else
FC_REFLECT( bts::blockchain::name_config, (name)(is_delegate)(owner) )
FC_REFLECT( bts::blockchain::genesis_block_config,
            (supply)(precision)(timestamp)(base_name)(base_symbol)(base_description)(names)(balances) )
#endif
