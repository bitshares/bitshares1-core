#pragma once
#include <fc/crypto/elliptic.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

  struct name_config
  {
     name_config():is_delegate(false){}
     std::string               name;
     bool                      is_delegate;
     fc::ecc::public_key_data  owner;
  };
  struct genesis_block_config
  {
     genesis_block_config():supply(0) {}

     double                                                         supply;
     fc::time_point_sec                                             timestamp;
     std::vector<std::pair<bts::blockchain::pts_address,double>>    balances;
     std::vector< name_config >                                     names;
  };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::name_config, (name)(is_delegate)(owner) )
FC_REFLECT( bts::blockchain::genesis_block_config, (supply)(timestamp)(balances)(names) )
