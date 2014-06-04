#pragma once
#include <memory>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/block.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/net/node.hpp>

#include <fc/network/ip.hpp>
#include <fc/filesystem.hpp>


namespace bts { namespace rpc {
  namespace detail { class rpc_client_impl; }

    using namespace bts::blockchain;
    using namespace bts::wallet;

    typedef vector<std::pair<share_type,string> > balances;


    class rpc_client_api
    {
        public:
         enum generate_transaction_flag
         {
            sign_and_broadcast    = 0,
            do_not_broadcast      = 1,
            do_not_sign           = 2
         };

        public:
         virtual              void wallet_rescan_blockchain_state() = 0;
         virtual              void wallet_import_bitcoin(const fc::path& filename,const string& passphrase, const string& account_name ) = 0;

         virtual vector<asset_record> blockchain_get_assets(const string& first_symbol, uint32_t count) const = 0;
    };

} } // bts::rpc
