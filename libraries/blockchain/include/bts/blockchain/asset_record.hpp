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
      supply_unlimit        = 1 << 4  ///<! The issuer can change the supply at will
   };

   struct asset_record;
   typedef fc::optional<asset_record> oasset_record;

   class chain_interface;
   struct asset_record
   {
      enum
      {
          god_issuer_id     =  0,
          null_issuer_id    = -1,
          market_issuer_id  = -2
      };

      bool is_market_issued()const      { return issuer_account_id == market_issuer_id; };
      bool is_user_issued()const        { return issuer_account_id > god_issuer_id; };

      bool is_retractable()const        { return is_user_issued() && (flags & retractable); }
      bool is_restricted()const         { return is_user_issued() && (flags & restricted); }
      bool is_market_frozen()const      { return is_user_issued() && (flags & market_halt); }
      bool is_balance_frozen()const     { return is_user_issued() && (flags & balance_halt); }
      bool is_supply_unlimited()const   { return is_user_issued() && (flags & supply_unlimit); }

      bool can_issue( const asset& amount )const;
      bool can_issue( const share_type amount )const;
      share_type available_shares()const;

      asset asset_from_string( const string& amount )const;
      string amount_to_string( share_type amount, bool append_symbol = true )const;

      asset_id_type       id;
      std::string         symbol;
      std::string         name;
      std::string         description;
      fc::variant         public_data;
      account_id_type     issuer_account_id = null_issuer_id;
      uint64_t            precision = 0;
      fc::time_point_sec  registration_date;
      fc::time_point_sec  last_update;
      share_type          maximum_share_supply = 0;
      share_type          current_share_supply = 0;
      share_type          collected_fees = 0;
      uint32_t            flags = 0;
      uint32_t            issuer_permissions = -1;

      /**
       *  The issuer can specify a transaction fee (of the asset type)
       *  that will be paid to the issuer with every transaction that
       *  references this asset type.
       */
      share_type          transaction_fee = 0;
      /**
       * 0 for no fee, 10000 for 100% fee.
       * This is used for gateways that want to continue earning market trading fees
       * when their assets are used.
       */
      uint16_t            market_fee = 0;
      multisig_meta_info  authority;

      void sanity_check( const chain_interface& )const;
      static oasset_record lookup( const chain_interface&, const asset_id_type );
      static oasset_record lookup( const chain_interface&, const string& );
      static void store( chain_interface&, const asset_id_type, const asset_record& );
      static void remove( chain_interface&, const asset_id_type );
   };

   class asset_db_interface
   {
      friend struct asset_record;

      virtual oasset_record asset_lookup_by_id( const asset_id_type )const = 0;
      virtual oasset_record asset_lookup_by_symbol( const string& )const = 0;

      virtual void asset_insert_into_id_map( const asset_id_type, const asset_record& ) = 0;
      virtual void asset_insert_into_symbol_map( const string&, const asset_id_type ) = 0;

      virtual void asset_erase_from_id_map( const asset_id_type ) = 0;
      virtual void asset_erase_from_symbol_map( const string& ) = 0;
   };

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::asset_permissions,
        (none)
        (retractable)
        (restricted)
        (market_halt)
        (balance_halt)
        (supply_unlimit)
        )
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
        (maximum_share_supply)
        (current_share_supply)
        (collected_fees)
        (flags)
        (issuer_permissions)
        (transaction_fee)
        (market_fee)
        (authority)
        )
