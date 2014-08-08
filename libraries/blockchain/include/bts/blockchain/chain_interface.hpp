#pragma once

#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/asset_record.hpp>
#include <bts/blockchain/balance_record.hpp>
#include <bts/blockchain/block_record.hpp>
#include <bts/blockchain/delegate_slate.hpp>
#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/proposal_record.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

   enum chain_property_enum
   {
      last_asset_id            = 0,
      last_account_id          = 1,
      last_proposal_id         = 2,
      last_random_seed_id      = 3,
      active_delegate_list_id  = 4,
      chain_id                 = 5, // hash of initial state
      /**
       *  N = num delegates
       *  Initial condition = 2N
       *  Every time a block is produced subtract 1
       *  Every time a block is missed add 2
       *  Maximum value is 2N, Min value is 0
       *
       *  Defines how many blocks you must wait to
       *  be 'confirmed' assuming that at least
       *  60% of the blocks in the last 2 rounds
       *  are present. Less than 60% and you
       *  are on the minority chain.
       */
      confirmation_requirement = 6,
      database_version         = 7, // database version, to know when we need to upgrade
      current_fee_rate         = 8, // database version, to know when we need to upgrade
      accumulated_fees         = 9, // database version, to know when we need to upgrade
      dirty_markets            = 10,
      last_feed_id             = 11 // used for allocating new data feeds
   };
   typedef uint32_t chain_property_type;

   const static short MAX_RECENT_OPERATIONS = 20;

   /**
    *  @class chain_interface
    *  @brief Abstracts the difference between the chain_database and pending_chain_state
    *
    */
   class chain_interface
   {
      public:
         virtual ~chain_interface(){};
         /** return the timestamp from the most recent block */
         virtual fc::time_point_sec         now()const                                              = 0;

         std::vector<account_id_type>       get_active_delegates()const;
         void                               set_active_delegates( const std::vector<account_id_type>& id );
         bool                               is_active_delegate( account_id_type ) const;

         virtual bool                       is_valid_symbol_name( const string& name ) const;
         virtual bool                       is_valid_account_name( const string& name ) const;

         /** convers an asset + asset_id to a more friendly representation using the symbol name */
         string                             to_pretty_asset( const asset& a )const;
         double                             to_pretty_price_double( const price& a )const;
         string                             to_pretty_price( const price& a )const;

         virtual oprice                     get_median_delegate_price( asset_id_type )const = 0;
         virtual void                       set_feed( const feed_record&  ) = 0;
         virtual ofeed_record               get_feed( const feed_index& )const = 0;
         virtual void                       set_market_dirty( asset_id_type quote_id, asset_id_type base_id ) = 0;

         virtual fc::ripemd160              get_current_random_seed()const                          = 0;

         share_type                         get_delegate_pay_rate()const;
         share_type                         get_accumulated_fees()const;
         void                               set_accumulated_fees( share_type fees );

         share_type                         get_fee_rate()const;
         void                               set_fee_rate( share_type fees );

         /** return the current fee rate in millishares */
         virtual odelegate_slate            get_delegate_slate( slate_id_type id )const             = 0;
         virtual void                       store_delegate_slate( slate_id_type id,
                                                                  const delegate_slate& slate )     = 0;

         virtual share_type                 get_delegate_registration_fee()const;
         virtual share_type                 get_asset_registration_fee()const;
         virtual share_type                 calculate_data_fee(size_t bytes) const;

         virtual int64_t                    get_required_confirmations()const;
         virtual fc::variant                get_property( chain_property_enum property_id )const    = 0;
         virtual void                       set_property( chain_property_enum property_id,
                                                          const fc::variant& property_value )       = 0;

         virtual omarket_status             get_market_status( asset_id_type quote_id, asset_id_type base_id ) = 0;
         virtual void                       store_market_status( const market_status& s ) = 0;

         virtual omarket_order              get_lowest_ask_record( asset_id_type quote_id, asset_id_type base_id ) = 0;
         virtual oorder_record              get_bid_record( const market_index_key& )const          = 0;
         virtual oorder_record              get_ask_record( const market_index_key& )const          = 0;
         virtual oorder_record              get_short_record( const market_index_key& )const        = 0;
         virtual ocollateral_record         get_collateral_record( const market_index_key& )const   = 0;

         virtual void                       store_bid_record( const market_index_key& key,
                                                              const order_record& )                 = 0;

         virtual void                       store_ask_record( const market_index_key& key,
                                                              const order_record& )                 = 0;

         virtual void                       store_short_record( const market_index_key& key,
                                                                const order_record& )               = 0;

         virtual void                       store_collateral_record( const market_index_key& key,
                                                                     const collateral_record& )     = 0;


         virtual oasset_record              get_asset_record( asset_id_type id )const               = 0;
         virtual obalance_record            get_balance_record( const balance_id_type& id )const    = 0;
         virtual oaccount_record            get_account_record( account_id_type id )const           = 0;
         virtual oaccount_record            get_account_record( const address& owner )const         = 0;

         virtual bool                       is_known_transaction( const transaction_id_type& trx_id ) = 0;

         virtual otransaction_record        get_transaction( const transaction_id_type& trx_id,
                                                             bool exact = true )const               = 0;

         virtual void                       store_transaction( const transaction_id_type&,
                                                                const transaction_record&  )        = 0;

         virtual oasset_record              get_asset_record( const std::string& symbol )const      = 0;
         virtual oaccount_record            get_account_record( const std::string& name )const      = 0;

         virtual void                       store_proposal_record( const proposal_record& r )       = 0;
         virtual oproposal_record           get_proposal_record( proposal_id_type id )const         = 0;

         virtual void                       store_proposal_vote( const proposal_vote& r )           = 0;
         virtual oproposal_vote             get_proposal_vote( proposal_vote_id_type id )const      = 0;

         virtual void                       store_asset_record( const asset_record& r )             = 0;
         virtual void                       store_balance_record( const balance_record& r )         = 0;
         virtual void                       store_account_record( const account_record& r )         = 0;

         virtual void                       store_recent_operation( const operation& o )            = 0;
         virtual vector<operation>          get_recent_operations( operation_type_enum t )          = 0;

         virtual void                       apply_deterministic_updates(){}

         virtual asset_id_type              last_asset_id()const;
         virtual asset_id_type              new_asset_id();

         virtual account_id_type            last_account_id()const;
         virtual account_id_type            new_account_id();

         virtual proposal_id_type           last_proposal_id()const;
         virtual proposal_id_type           new_proposal_id();

         virtual uint32_t                   get_head_block_num()const                               = 0;

         virtual void                       store_slot_record( const slot_record& r )               = 0;
         virtual oslot_record               get_slot_record( const time_point_sec& start_time )const= 0;

         virtual void                       store_market_history_record( const market_history_key& key,
                                                                  const market_history_record& record ) = 0;
         virtual omarket_history_record     get_market_history_record( const market_history_key& key ) const = 0;

         virtual map<asset_id_type, asset_id_type>  get_dirty_markets()const;
         virtual void                               set_dirty_markets( const map<asset_id_type,asset_id_type>& );

         virtual void                       set_market_transactions( vector<market_transaction> trxs ) = 0;
   };
   typedef std::shared_ptr<chain_interface> chain_interface_ptr;

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::chain_property_enum,
                 (last_asset_id)
                 (last_account_id)
                 (last_proposal_id)
                 (last_random_seed_id)
                 (active_delegate_list_id)
                 (chain_id)
                 (confirmation_requirement)
                 (database_version)
                 (current_fee_rate)
                 (accumulated_fees)
                 (dirty_markets)
                 (last_feed_id)
                 )
