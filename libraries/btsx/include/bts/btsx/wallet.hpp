#pragma once
#include <bts/wallet/wallet.hpp>

namespace bts { namespace btsx {
   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   class wallet : public bts::wallet::wallet 
   {
      public:
         wallet();
         ~wallet();

      protected:
         virtual void dump_output( const trx_output& out );
         virtual bool scan_output( const trx_output& out, const output_reference& ref, const bts::wallet::output_index& idx );

      private:
         std::unique_ptr<detail::wallet_impl> my;
   };

} } // bts::btsx
