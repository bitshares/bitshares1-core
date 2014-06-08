#pragma once
#include <fc/crypto/elliptic.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

  struct name_config
  {
     name_config():is_delegate(false){}
     std::string               name;
     bool                      is_delegate;
     public_key_type           owner;
  };
  struct genesis_block_config
  {
     genesis_block_config():supply(0),precision(1000000) {}

     int64_t                                                        supply;
     int64_t                                                        precision;
     fc::time_point_sec                                             timestamp;
     std::string                                                    base_symbol;
     std::string                                                    base_name;
     std::string                                                    base_description;
     std::vector<std::pair<bts::blockchain::pts_address,double>>    balances;
     std::vector< name_config >                                     names;
  };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::name_config, (name)(is_delegate)(owner) )
FC_REFLECT( bts::blockchain::genesis_block_config, 
            (supply)
            (precision)
            (timestamp)
            (base_name)
            (base_symbol)
            (base_description) 
            (names)
            (balances)
          )
