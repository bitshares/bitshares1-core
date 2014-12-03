#pragma once

#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

   enum asset_permissions
   {
      none                  = 0,
      retractable           = 1 << 0, ///<! The issuer can sign inplace of the owner
      restricted            = 1 << 1, ///<! The issuer whitelists public keys
      market_halt           = 1 << 2, ///<! The issuer can/did freeze all markets
      balance_halt          = 1 << 3, ///<! The issuer can/did freeze all balances
      supply_unlimit        = 1 << 4, ///<! The issuer can change the supply at will
      default_permissions   = retractable | market_halt | balance_halt | supply_unlimit,
      all_permissions       = 0xffffffff
   };

   struct asset_record
   {
      enum
      {
          god_issuer_id     =  0,
          null_issuer_id    = -1,
          market_issuer_id  = -2
      };

      asset_record make_null()const;
      bool is_null()const               { return issuer_account_id == null_issuer_id; };

      bool is_market_issued()const      { return issuer_account_id == market_issuer_id; };
      bool is_user_issued()const        { return issuer_account_id > god_issuer_id; };

      bool is_retractable()const        { return is_user_issued() && (flags & retractable); }
      bool is_restricted()const         { return is_user_issued() && (flags & restricted); }
      bool is_market_frozen()const      { return is_user_issued() && (flags & market_halt); }
      bool is_balance_frozen()const     { return is_user_issued() && (flags & balance_halt); }
      bool is_supply_unlimited()const   { return is_user_issued() && (flags & supply_unlimit); }

      bool can_issue( const asset& amount )const;
      bool can_issue( const share_type& amount )const;
      share_type available_shares()const;

      asset_id_type       id;
      std::string         symbol;
      std::string         name;
      std::string         description;
      fc::variant         public_data;
      account_id_type     issuer_account_id;
      uint64_t            precision = 0;
      fc::time_point_sec  registration_date;
      fc::time_point_sec  last_update;
      share_type          current_share_supply = 0;
      share_type          maximum_share_supply = 0;
      share_type          collected_fees = 0;
      uint32_t            issuer_permissions = default_permissions;
      uint32_t            flags = retractable;
      /**
       *  The issuer can specify a transaction fee (of the asset type)
       *  that will be paid to the issuer with every transaction that
       *  references this asset type.
       */
      share_type          transaction_fee = 0;
      multisig_meta_info  authority;

      /** reserved for future extensions */
      vector<char>        reserved;
   };
   typedef fc::optional<asset_record> oasset_record;

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::asset_permissions,
    (none)(retractable)(restricted)(market_halt)(balance_halt)(supply_unlimit)(all_permissions)(default_permissions) )

FC_REFLECT( bts::blockchain::asset_record,
            (id)
            (symbol)
            (name)
            (description)
            (public_data)
            (issuer_account_id)
            (precision)
            (registration_date)
            (last_update)
            (current_share_supply)
            (maximum_share_supply)
            (collected_fees)
            (issuer_permissions)
            (flags)
            (transaction_fee)
            (authority)
           )
