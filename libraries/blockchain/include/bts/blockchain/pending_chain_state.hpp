#pragma once
#include <bts/blockchain/chain_interface.hpp>
#include <fc/reflect/reflect.hpp>
#include <deque>

namespace bts { namespace blockchain {

   class pending_chain_state : public chain_interface, public std::enable_shared_from_this<pending_chain_state>
   {
      public:
                                        pending_chain_state( chain_interface_ptr prev_state = chain_interface_ptr() );
                                        pending_chain_state( const pending_chain_state& cpy );
         void                           init();
         pending_chain_state&           operator=(const pending_chain_state&) = default;

         virtual                        ~pending_chain_state()override;

         void                           set_prev_state( chain_interface_ptr prev_state );

         void                           authorize( asset_id_type asset_id, const address& owner, object_id_type oid = 0 ) override;
         optional<object_id_type>       get_authorization( asset_id_type asset_id, const address& owner )const override;

         virtual void                   set_market_dirty( const asset_id_type quote_id, const asset_id_type base_id )override;

         virtual fc::time_point_sec     now()const override;

         virtual void                       store_asset_proposal( const proposal_record& r ) override;
         virtual optional<proposal_record>  fetch_asset_proposal( asset_id_type asset_id, proposal_id_type proposal_id )const override;

         virtual void                   store_burn_record( const burn_record& br ) override;
         virtual oburn_record           fetch_burn_record( const burn_record_key& key )const override;

         virtual oprice                 get_active_feed_price( const asset_id_type quote_id,
                                                               const asset_id_type base_id = 0 )const override;

         virtual bool                   is_known_transaction( const transaction& trx )const override;
         virtual otransaction_record    get_transaction( const transaction_id_type& trx_id, bool exact = true )const override;

         virtual void                   store_transaction( const transaction_id_type&, const transaction_record&  ) override;

         virtual omarket_status         get_market_status( const asset_id_type quote_id, const asset_id_type base_id )override;
         virtual void                   store_market_status( const market_status& s ) override;

         virtual omarket_order          get_lowest_ask_record( const asset_id_type quote_id,
                                                               const asset_id_type base_id )override;
         virtual oorder_record          get_bid_record( const market_index_key& )const override;
         virtual oorder_record          get_ask_record( const market_index_key& )const override;
         virtual oorder_record          get_relative_bid_record( const market_index_key& )const override;
         virtual oorder_record          get_relative_ask_record( const market_index_key& )const override;
         virtual oorder_record          get_short_record( const market_index_key& )const override;
         virtual ocollateral_record     get_collateral_record( const market_index_key& )const override;

         virtual void                   store_bid_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_ask_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_relative_bid_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_relative_ask_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_short_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_collateral_record( const market_index_key& key, const collateral_record& ) override;

         virtual void                   store_object_record( const object_record& obj )override;
         virtual oobject_record         get_object_record( const object_id_type id )const override;


         virtual void                       store_site_record( const site_record& site )override;
         virtual osite_record               lookup_site( const string& site_name)const override;

         virtual void                       store_edge_record( const object_record& edge )override;
         virtual oobject_record               get_edge( const object_id_type from,
                                                      const object_id_type to,
                                                      const string& name )const          override;
         virtual map<string, object_record>   get_edges( const object_id_type from,
                                                       const object_id_type to )const   override;
         virtual map<object_id_type, map<string, object_record>>
                                        get_edges( const object_id_type from )const override;

         virtual void                   store_market_history_record( const market_history_key& key,
                                                                     const market_history_record& record )override;
         virtual omarket_history_record get_market_history_record( const market_history_key& key )const override;

         /** apply changes from this pending state to the previous state */
         virtual void                   apply_changes()const;

         /** populate undo state with everything that would be necessary to revert this
          * pending state to the previous state.
          */
         virtual void                   get_undo_state( const chain_interface_ptr& undo_state )const;

         template<typename T, typename U>
         void populate_undo_state( const chain_interface_ptr& undo_state, const chain_interface_ptr& prev_state,
                                   const T& store_map, const U& remove_set )const
         {
             using V = typename T::mapped_type;
             for( const auto& key : remove_set )
             {
                 const auto prev_record = prev_state->lookup<V>( key );
                 if( prev_record.valid() ) undo_state->store( key, *prev_record );
             }
             for( const auto& item : store_map )
             {
                 const auto& key = item.first;
                 const auto prev_record = prev_state->lookup<V>( key );
                 if( prev_record.valid() ) undo_state->store( key, *prev_record );
                 else undo_state->remove<V>( key );
             }
         }

         template<typename T, typename U>
         void apply_records( const chain_interface_ptr& prev_state, const T& store_map, const U& remove_set )const
         {
             using V = typename T::mapped_type;
             for( const auto& key : remove_set ) prev_state->remove<V>( key );
             for( const auto& item : store_map ) prev_state->store( item.first, item.second );
         }

         /** load the state from a variant */
         virtual void                   from_variant( const variant& v );
         /** convert the state to a variant */
         virtual variant                to_variant()const;

         virtual uint32_t               get_head_block_num()const override;

         virtual void                   set_market_transactions( vector<market_transaction> trxs )override;

         map<property_id_type, property_record>                             _property_id_to_record;
         set<property_id_type>                                              _property_id_remove;

         unordered_map<account_id_type, account_record>                     _account_id_to_record;
         unordered_set<account_id_type>                                     _account_id_remove;
         unordered_map<string, account_id_type>                             _account_name_to_id;
         unordered_map<address, account_id_type>                            _account_address_to_id;

         unordered_map<asset_id_type, asset_record>                         _asset_id_to_record;
         unordered_set<asset_id_type>                                       _asset_id_remove;
         unordered_map<string, asset_id_type>                               _asset_symbol_to_id;

         unordered_map<slate_id_type, slate_record>                         _slate_id_to_record;
         unordered_set<slate_id_type>                                       _slate_id_remove;

         unordered_map<balance_id_type, balance_record>                     _balance_id_to_record;
         unordered_set<balance_id_type>                                     _balance_id_remove;

         unordered_map<transaction_id_type, transaction_record>             _transaction_id_to_record;
         unordered_set<transaction_id_type>                                 _transaction_id_remove;
         unordered_set<digest_type>                                         _transaction_digests;

         map<feed_index, feed_record>                                       _feed_index_to_record;
         set<feed_index>                                                    _feed_index_remove;

         map<slot_index, slot_record>                                       _slot_index_to_record;
         set<slot_index>                                                    _slot_index_remove;
         map<time_point_sec, account_id_type>                               _slot_timestamp_to_delegate;

         map<burn_record_key,burn_record_value>                             burns;

         map< market_index_key, order_record>                               asks;
         map< market_index_key, order_record>                               bids;
         map< market_index_key, order_record>                               relative_asks;
         map< market_index_key, order_record>                               relative_bids;
         map< market_index_key, order_record>                               shorts;
         map< market_index_key, collateral_record>                          collateral;

         std::set<std::pair<asset_id_type, asset_id_type>>                  _dirty_markets;

         vector<market_transaction>                                         market_transactions;
         map< std::pair<asset_id_type,asset_id_type>, market_status>        market_statuses;
         map<market_history_key, market_history_record>                     market_history;

         map< object_id_type, object_record >                               objects;
         map< edge_index_key, object_id_type >                              edge_index;
         map< edge_index_key, object_id_type >                              reverse_edge_index;

         map< string, site_record >                                         site_index;

         map< std::pair<asset_id_type,address>, object_id_type >            authorizations;
         map< std::pair<asset_id_type,proposal_id_type>, proposal_record >  asset_proposals;

      private:
         // Not serialized
         std::weak_ptr<chain_interface>                                     _prev_state;

         virtual void init_property_db_interface()override;
         virtual void init_account_db_interface()override;
         virtual void init_asset_db_interface()override;
         virtual void init_balance_db_interface()override;
         virtual void init_transaction_db_interface()override;
         virtual void init_slate_db_interface()override;
         virtual void init_feed_db_interface()override;
         virtual void init_slot_db_interface()override;
   };
   typedef std::shared_ptr<pending_chain_state> pending_chain_state_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::pending_chain_state,
        (_property_id_to_record)
        (_property_id_remove)
        (_account_id_to_record)
        (_account_id_remove)
        (_account_name_to_id)
        (_account_address_to_id)
        (_asset_id_to_record)
        (_asset_id_remove)
        (_asset_symbol_to_id)
        (_slate_id_to_record)
        (_slate_id_remove)
        (_balance_id_to_record)
        (_balance_id_remove)
        (_transaction_id_to_record)
        (_transaction_id_remove)
        (_transaction_digests)
        (_feed_index_to_record)
        (_feed_index_remove)
        (_slot_index_to_record)
        (_slot_index_remove)
        (_slot_timestamp_to_delegate)
        (burns)
        (asks)
        (bids)
        (relative_asks)
        (relative_bids)
        (shorts)
        (collateral)
        (_dirty_markets)
        (market_transactions)
        (market_statuses)
        (market_history)
        (objects)
        (edge_index)
        (reverse_edge_index)
        (site_index)
        (authorizations)
        (asset_proposals)
    )
