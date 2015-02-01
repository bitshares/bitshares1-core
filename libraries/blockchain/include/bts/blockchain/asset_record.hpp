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

   class chain_interface;
   struct asset_db_interface;
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

      asset_id_type       id;
      std::string         symbol;
      std::string         name;
      std::string         description;
      fc::variant         public_data;
      account_id_type     issuer_account_id = null_issuer_id;
      uint64_t            precision = 0;
      fc::time_point_sec  registration_date;
      fc::time_point_sec  last_update;
      share_type          current_share_supply = 0;
      share_type          maximum_share_supply = 0;
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
       * 0 for no fee, 10000 for 10% fee.
       * This is used for gateways that want to continue earning market trading fees
       * when their assets are used.
       */
      uint16_t            market_fee = BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE; 
      multisig_meta_info  authority;

      proposal_id_type    last_proposal_id = 0;


      /** reserved for future extensions */
      vector<char>        reserved;

      static const asset_db_interface& db_interface( const chain_interface& );

      asset asset_from_string( const string& amount )const;
      string amount_to_string( share_type amount, bool append_symbol = true )const;
   };
   typedef fc::optional<asset_record> oasset_record;

   struct asset_db_interface
   {
       std::function<oasset_record( const asset_id_type )>              lookup_by_id;
       std::function<oasset_record( const string& )>                    lookup_by_symbol;

       std::function<void( const asset_id_type, const asset_record& )>  insert_into_id_map;
       std::function<void( const string&, const asset_id_type )>        insert_into_symbol_map;

       std::function<void( const asset_id_type )>                       erase_from_id_map;
       std::function<void( const string& )>                             erase_from_symbol_map;

       oasset_record lookup( const asset_id_type )const;
       oasset_record lookup( const string& )const;
       void store( const asset_id_type, const asset_record& )const;
       void remove( const asset_id_type )const;
   };

   struct proposal_record
   {
      proposal_record(){}

      proposal_record( asset_id_type id, proposal_id_type pid, object_id_type oid = -1 )
      :asset_id(id),proposal_id(pid),info(oid){}

      proposal_record make_null()const { auto tmp = *this; tmp.info = -1; return tmp; }
      std::pair<asset_id_type,proposal_id_type> key()const{ return std::make_pair(asset_id,proposal_id); }

      asset_id_type      asset_id      = 0;
      proposal_id_type   proposal_id   = 0;
      object_id_type     info          = -1;
      uint32_t           votes_for     = 0;
      uint32_t           votes_against = 0;
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
FC_REFLECT( bts::blockchain::proposal_record,
        (asset_id)
        (proposal_id)
        (info)
        (votes_for)
        (votes_against)
        );

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
        (flags)
        (issuer_permissions)
        (transaction_fee)
        (market_fee)
        (authority)
        (last_proposal_id)
        )
