#pragma once

#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/asset_record.hpp>
#include <bts/blockchain/balance_record.hpp>
#include <bts/blockchain/block_record.hpp>
#include <bts/blockchain/condition.hpp>
#include <bts/blockchain/edge_record.hpp>
#include <bts/blockchain/feed_record.hpp>
#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/site_record.hpp>
#include <bts/blockchain/slate_record.hpp>
#include <bts/blockchain/slot_record.hpp>
#include <bts/blockchain/transaction_record.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

   enum chain_property_enum
   {
      database_version         = 0,
      chain_id                 = 1,
      last_asset_id            = 2,
      last_account_id          = 3,
      active_delegate_list_id  = 4,
      last_random_seed_id      = 5,
      statistics_enabled       = 6,
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
      confirmation_requirement = 7,
      dirty_markets            = 8,
      last_object_id           = 9
   };
   typedef uint32_t chain_property_type;

   /**
    *  @class chain_interface
    *  @brief Abstracts the difference between the chain_database and pending_chain_state
    *
    */
   class chain_interface
   {
      public:
         virtual ~chain_interface(){};

         virtual fc::time_point_sec now()const = 0;

         chain_interface(){}

         // don't copy member variables
         chain_interface( const chain_interface& ){} 
         chain_interface& operator=(const chain_interface&){ return *this; }; 

         optional<string>                   get_parent_account_name( const string& account_name )const;
         static bool                        is_valid_account_name( const string& name );
         bool                               is_valid_symbol_name( const string& symbol )const;
         bool                               is_valid_symbol_name_v1( const string& symbol )const;

         bool                               is_fraudulent_asset( const asset_record& suspect_record )const;

         time_point_sec                     get_genesis_timestamp()const;

         share_type                         get_max_delegate_pay_issued_per_block()const;
         share_type                         get_delegate_registration_fee( uint8_t pay_rate )const;
         share_type                         get_asset_registration_fee( uint8_t symbol_length )const;

         share_type                         get_delegate_registration_fee_v1( uint8_t pay_rate )const;
         share_type                         get_asset_registration_fee_v1()const;
         share_type                         get_delegate_pay_rate_v1()const;

         vector<account_id_type>            get_active_delegates()const;
         void                               set_active_delegates( const std::vector<account_id_type>& id );
         bool                               is_active_delegate( const account_id_type id )const;

         virtual void                       authorize( asset_id_type asset_id, const address& owner, object_id_type oid = 0 ) = 0;
         void                               deauthorize( asset_id_type asset_id, const address& owner ) { authorize( asset_id, owner, -1 ); }
         virtual optional<object_id_type>   get_authorization( asset_id_type asset_id, const address& owner )const = 0;

         virtual void                       store_asset_proposal( const proposal_record& r ) = 0;
         virtual optional<proposal_record>  fetch_asset_proposal( asset_id_type asset_id, proposal_id_type proposal_id )const = 0;

         /** converts an asset + asset_id to a more friendly representation using the symbol name */
         string                             to_pretty_asset( const asset& a )const;
         double                             to_pretty_price_double( const price& a )const;
         string                             to_pretty_price( const price& a )const;
         /** converts a numeric string + asset symbol to an asset */
         asset                              to_ugly_asset( const string& amount, const string& symbol )const;
         /** converts a numeric string and two asset symbols to a price */
         price                              to_ugly_price( const string& price_string,
                                                           const string& base_symbol,
                                                           const string& quote_symbol,
                                                           bool do_precision_dance = true )const;

         void                               set_chain_id( const digest_type& id );
         digest_type                        get_chain_id()const;

         void                               set_statistics_enabled( const bool enabled );
         bool                               get_statistics_enabled()const;

         virtual void                       store_burn_record( const burn_record& br ) = 0;
         virtual oburn_record               fetch_burn_record( const burn_record_key& key )const = 0;

         virtual oprice                     get_active_feed_price( const asset_id_type quote_id,
                                                                   const asset_id_type base_id = 0 )const = 0;

         virtual void                       set_market_dirty( const asset_id_type quote_id,
                                                              const asset_id_type base_id )                = 0;

         virtual fc::ripemd160              get_current_random_seed()const                                  = 0;

         virtual optional<variant>          get_property( chain_property_enum property_id )const            = 0;
         virtual void                       set_property( chain_property_enum property_id,
                                                          const fc::variant& property_value )               = 0;

         virtual void                       set_required_confirmations( uint64_t );
         virtual uint64_t                   get_required_confirmations()const;

         virtual omarket_status             get_market_status( const asset_id_type quote_id,
                                                               const asset_id_type base_id )               = 0;
         virtual void                       store_market_status( const market_status& s )                   = 0;

         virtual omarket_order              get_lowest_ask_record( const asset_id_type quote_id,
                                                                   const asset_id_type base_id )           = 0;
         virtual oorder_record              get_bid_record( const market_index_key& )const                  = 0;
         virtual oorder_record              get_ask_record( const market_index_key& )const                  = 0;
         virtual oorder_record              get_relative_bid_record( const market_index_key& )const         = 0;
         virtual oorder_record              get_relative_ask_record( const market_index_key& )const         = 0;
         virtual oorder_record              get_short_record( const market_index_key& )const                = 0;
         virtual ocollateral_record         get_collateral_record( const market_index_key& )const           = 0;

         virtual void                       store_bid_record( const market_index_key& key,
                                                              const order_record& )                         = 0;

         virtual void                       store_ask_record( const market_index_key& key,
                                                              const order_record& )                         = 0;

         virtual void                       store_relative_bid_record( const market_index_key& key,
                                                                       const order_record& )                = 0;

         virtual void                       store_relative_ask_record( const market_index_key& key,
                                                                       const order_record& )                = 0;

         virtual void                       store_short_record( const market_index_key& key,
                                                                const order_record& )                       = 0;

         virtual void                       store_collateral_record( const market_index_key& key,
                                                                     const collateral_record& )             = 0;


         virtual bool                       is_known_transaction( const transaction& trx )const             = 0;

         virtual otransaction_record        get_transaction( const transaction_id_type& trx_id,
                                                             bool exact = true )const                       = 0;

         virtual void                       store_transaction( const transaction_id_type&,
                                                                const transaction_record&  )                = 0;

         virtual void                       store_object_record( const object_record& obj )                 = 0;
         virtual oobject_record             get_object_record( const object_id_type id )const              = 0;

         virtual void                       store_edge_record( const object_record& edge )                  = 0;

         virtual void                       store_site_record( const site_record& edge )                    = 0;

         oobject_record                       get_edge( const object_id_type id );
         virtual oobject_record               get_edge( const object_id_type from,
                                                      const object_id_type to,
                                                      const string& name )const                             = 0;
         virtual map<string, object_record>   get_edges( const object_id_type from,
                                                       const object_id_type to )const                      = 0;
         virtual map<object_id_type, map<string, object_record>>
                                            get_edges( const object_id_type from )const                    = 0;

         virtual osite_record               lookup_site( const string& site_name) const                    = 0;

         virtual asset_id_type              last_asset_id()const;
         virtual asset_id_type              new_asset_id();

         virtual account_id_type            last_account_id()const;
         virtual account_id_type            new_account_id();

         virtual object_id_type             last_object_id()const;
         virtual object_id_type             new_object_id( obj_type type );

         virtual multisig_condition         get_object_condition( const object_id_type id, int depth = 0 );
         virtual multisig_condition         get_object_condition( const object_record& obj, int depth = 0 );
         virtual object_id_type             get_owner_object( const object_id_type obj );

         virtual uint32_t                   get_head_block_num()const                                       = 0;

         virtual void                       store_market_history_record( const market_history_key& key,
                                                                         const market_history_record& record ) = 0;
         virtual omarket_history_record     get_market_history_record( const market_history_key& key )const = 0;

         virtual void set_dirty_markets( const std::set<std::pair<asset_id_type, asset_id_type>>& );
         virtual std::set<std::pair<asset_id_type, asset_id_type>> get_dirty_markets()const;

         virtual void                       set_market_transactions( vector<market_transaction> trxs )      = 0;

         oaccount_record                    get_account_record( const account_id_type id )const;
         oaccount_record                    get_account_record( const string& name )const;
         oaccount_record                    get_account_record( const address& addr )const;
         void                               store_account_record( const account_record& record );

         oasset_record                      get_asset_record( const asset_id_type id )const;
         oasset_record                      get_asset_record( const string& symbol )const;
         void                               store_asset_record( const asset_record& record );

         obalance_record                    get_balance_record( const balance_id_type& id )const;
         void                               store_balance_record( const balance_record& record );

         oslate_record                      get_slate_record( const slate_id_type id )const;
         void                               store_slate_record( const slate_record& record );

         ofeed_record                       get_feed_record( const feed_index index )const;
         virtual void                       store_feed_record( const feed_record& record );

         oslot_record                       get_slot_record( const slot_index index )const;
         oslot_record                       get_slot_record( const time_point_sec timestamp )const;
         void                               store_slot_record( const slot_record& record );

         template<typename T, typename U>
         optional<T> lookup( const U& key )const
         { try {
             return T::db_interface( *this ).lookup( key );
         } FC_CAPTURE_AND_RETHROW( (key) ) }

         template<typename T, typename U>
         void store( const U& key, const T& record )
         { try {
             T::db_interface( *this ).store( key, record );
         } FC_CAPTURE_AND_RETHROW( (key)(record) ) }

         template<typename T, typename U>
         void remove( const U& key )
         { try {
             T::db_interface( *this ).remove( key );
         } FC_CAPTURE_AND_RETHROW( (key) ) }

      protected:
         friend struct account_record;
         account_db_interface _account_db_interface;
         virtual void init_account_db_interface() = 0;

         friend struct asset_record;
         asset_db_interface _asset_db_interface;
         virtual void init_asset_db_interface() = 0;

         friend struct balance_record;
         balance_db_interface _balance_db_interface;
         virtual void init_balance_db_interface() = 0;

         friend struct transaction_record;
         transaction_db_interface _transaction_db_interface;
         virtual void init_transaction_db_interface() = 0;

         friend struct slate_record;
         slate_db_interface _slate_db_interface;
         virtual void init_slate_db_interface() = 0;

         friend struct feed_record;
         feed_db_interface _feed_db_interface;
         virtual void init_feed_db_interface() = 0;

         friend struct slot_record;
         slot_db_interface _slot_db_interface;
         virtual void init_slot_db_interface() = 0;
   };
   typedef std::shared_ptr<chain_interface> chain_interface_ptr;

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::chain_property_enum,
        (database_version)
        (chain_id)
        (last_asset_id)
        (last_account_id)
        (active_delegate_list_id)
        (last_random_seed_id)
        (statistics_enabled)
        (confirmation_requirement)
        (dirty_markets)
        (last_object_id)
        )
