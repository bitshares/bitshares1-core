#pragma once
#include <bts/blockchain/chain_interface.hpp>
#include <fc/reflect/reflect.hpp>
#include <deque>

namespace bts { namespace blockchain {

   class pending_chain_state : public chain_interface, public std::enable_shared_from_this<pending_chain_state>
   {
      public:
                                        pending_chain_state( chain_interface_ptr prev_state = chain_interface_ptr() );

         void                           set_prev_state( chain_interface_ptr prev_state );

         virtual fc::time_point_sec     now()const override;

         virtual bool                   is_known_transaction( const transaction& trx )const override;
         virtual otransaction_record    get_transaction( const transaction_id_type& trx_id, bool exact = true )const override;

         virtual void                   store_transaction( const transaction_id_type&, const transaction_record&  ) override;

         virtual void                   set_market_dirty( const asset_id_type quote_id, const asset_id_type base_id )override;

         virtual oprice                 get_active_feed_price( const asset_id_type quote_id )const override;

         virtual omarket_order          get_lowest_ask_record( const asset_id_type quote_id,
                                                               const asset_id_type base_id )override;
         virtual oorder_record          get_bid_record( const market_index_key& )const override;
         virtual oorder_record          get_ask_record( const market_index_key& )const override;
         virtual oorder_record          get_short_record( const market_index_key& )const override;
         virtual ocollateral_record     get_collateral_record( const market_index_key& )const override;

         virtual void                   store_bid_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_ask_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_short_record( const market_index_key& key, const order_record& ) override;
         virtual void                   store_collateral_record( const market_index_key& key, const collateral_record& ) override;

         virtual void                   store_market_history_record( const market_history_key& key,
                                                                     const market_history_record& record )override;
         virtual omarket_history_record get_market_history_record( const market_history_key& key )const override;

         void                           build_undo_state( const chain_interface_ptr& undo_state )const;
         void                           apply_changes()const;

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

         void                           from_variant( const variant& v );
         variant                        to_variant()const;

         virtual uint32_t               get_head_block_num()const override;

         virtual void                   set_market_transactions( vector<market_transaction> trxs )override;

         void                           check_supplies()const;

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

         map<burn_index, burn_record>                                       _burn_index_to_record;
         set<burn_index>                                                    _burn_index_remove;

         map<status_index, status_record>                                   _status_index_to_record;
         set<status_index>                                                  _status_index_remove;

         map<feed_index, feed_record>                                       _feed_index_to_record;
         set<feed_index>                                                    _feed_index_remove;

         map<slot_index, slot_record>                                       _slot_index_to_record;
         set<slot_index>                                                    _slot_index_remove;
         map<time_point_sec, account_id_type>                               _slot_timestamp_to_delegate;

         map< market_index_key, order_record>                               asks;
         map< market_index_key, order_record>                               bids;
         map< market_index_key, order_record>                               shorts;
         map< market_index_key, collateral_record>                          collateral;

         std::set<std::pair<asset_id_type, asset_id_type>>                  _dirty_markets;

         vector<market_transaction>                                         market_transactions;
         map<market_history_key, market_history_record>                     market_history;

      private:
         // Not serialized
         std::weak_ptr<chain_interface>                                     _prev_state;

         virtual oproperty_record property_lookup_by_id( const property_id_type )const override;
         virtual void property_insert_into_id_map( const property_id_type, const property_record& )override;
         virtual void property_erase_from_id_map( const property_id_type )override;

         virtual oaccount_record account_lookup_by_id( const account_id_type )const override;
         virtual oaccount_record account_lookup_by_name( const string& )const override;
         virtual oaccount_record account_lookup_by_address( const address& )const override;

         virtual void account_insert_into_id_map( const account_id_type, const account_record& )override;
         virtual void account_insert_into_name_map( const string&, const account_id_type )override;
         virtual void account_insert_into_address_map( const address&, const account_id_type )override;
         virtual void account_insert_into_vote_set( const vote_del& )override;

         virtual void account_erase_from_id_map( const account_id_type )override;
         virtual void account_erase_from_name_map( const string& )override;
         virtual void account_erase_from_address_map( const address& )override;
         virtual void account_erase_from_vote_set( const vote_del& )override;

         virtual oasset_record asset_lookup_by_id( const asset_id_type )const override;
         virtual oasset_record asset_lookup_by_symbol( const string& )const override;

         virtual void asset_insert_into_id_map( const asset_id_type, const asset_record& )override;
         virtual void asset_insert_into_symbol_map( const string&, const asset_id_type )override;

         virtual void asset_erase_from_id_map( const asset_id_type )override;
         virtual void asset_erase_from_symbol_map( const string& )override;

         virtual oslate_record slate_lookup_by_id( const slate_id_type )const override;
         virtual void slate_insert_into_id_map( const slate_id_type, const slate_record& )override;
         virtual void slate_erase_from_id_map( const slate_id_type )override;

         virtual obalance_record balance_lookup_by_id( const balance_id_type& )const override;
         virtual void balance_insert_into_id_map( const balance_id_type&, const balance_record& )override;
         virtual void balance_erase_from_id_map( const balance_id_type& )override;

         virtual otransaction_record transaction_lookup_by_id( const transaction_id_type& )const override;

         virtual void transaction_insert_into_id_map( const transaction_id_type&, const transaction_record& )override;
         virtual void transaction_insert_into_unique_set( const transaction& )override;

         virtual void transaction_erase_from_id_map( const transaction_id_type& )override;
         virtual void transaction_erase_from_unique_set( const transaction& )override;

         virtual oburn_record burn_lookup_by_index( const burn_index& )const override;
         virtual void burn_insert_into_index_map( const burn_index&, const burn_record& )override;
         virtual void burn_erase_from_index_map( const burn_index& )override;

         virtual ostatus_record status_lookup_by_index( const status_index )const override;
         virtual void status_insert_into_index_map( const status_index, const status_record& )override;
         virtual void status_erase_from_index_map( const status_index )override;

         virtual ofeed_record feed_lookup_by_index( const feed_index )const override;
         virtual void feed_insert_into_index_map( const feed_index, const feed_record& )override;
         virtual void feed_erase_from_index_map( const feed_index )override;

         virtual oslot_record slot_lookup_by_index( const slot_index )const override;
         virtual oslot_record slot_lookup_by_timestamp( const time_point_sec )const override;

         virtual void slot_insert_into_index_map( const slot_index, const slot_record& )override;
         virtual void slot_insert_into_timestamp_map( const time_point_sec, const account_id_type )override;

         virtual void slot_erase_from_index_map( const slot_index )override;
         virtual void slot_erase_from_timestamp_map( const time_point_sec )override;
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
            (_burn_index_to_record)
            (_burn_index_remove)
            (_status_index_to_record)
            (_status_index_remove)
            (_feed_index_to_record)
            (_feed_index_remove)
            (_slot_index_to_record)
            (_slot_index_remove)
            (_slot_timestamp_to_delegate)
            (asks)
            (bids)
            (shorts)
            (collateral)
            (_dirty_markets)
            (market_transactions)
            (market_history)
            )
