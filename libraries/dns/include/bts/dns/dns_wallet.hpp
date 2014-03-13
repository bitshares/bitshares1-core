#pragma once
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/dns/dns_db.hpp>

namespace bts { namespace dns {
    namespace detail  { class dns_wallet_impl; }

    class dns_wallet : public bts::wallet::wallet
    {
        public:
            bts::blockchain::signed_transaction buy_domain(const std::string& name,
                                          bts::blockchain::asset amount, dns_db& db);
            bts::blockchain::signed_transaction update_record(const std::string& name,
                                             bts::blockchain::address addr, fc::variant value);
            bts::blockchain::signed_transaction sell_domain(const std::string&, 
                                            bts::blockchain::asset amount);

        protected:
            virtual void scan_output( const bts::blockchain::trx_output& out,
                                      const bts::blockchain::output_reference& ref,
                                      const bts::wallet::output_index& oidx );

        private:
             std::unique_ptr<detail::dns_wallet_impl> my;
     };

} } // bts::dns 
