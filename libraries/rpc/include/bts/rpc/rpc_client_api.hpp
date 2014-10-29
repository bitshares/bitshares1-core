#pragma once

#include <bts/net/node.hpp>
#include <bts/wallet/wallet.hpp>

#include <fc/filesystem.hpp>
#include <fc/network/ip.hpp>

#include <memory>

namespace bts { namespace rpc {
  namespace detail { class rpc_client_impl; }

    using namespace bts::blockchain;
    using namespace bts::wallet;

    typedef vector<std::pair<share_type,string> > balances;

    enum generate_transaction_flag
    {
      sign_and_broadcast    = 0,
      do_not_broadcast      = 1,
      do_not_sign           = 2
    };

} } // bts::rpc
FC_REFLECT_ENUM( bts::rpc::generate_transaction_flag, (do_not_broadcast)(do_not_sign)(sign_and_broadcast) )
