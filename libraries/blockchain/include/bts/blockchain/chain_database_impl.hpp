#pragma once

#include <bts/blockchain/chain_database.hpp>
#include <bts/db/cached_level_map.hpp>
#include <bts/db/fast_level_map.hpp>
#include <fc/thread/mutex.hpp>

namespace bts { namespace blockchain {

   struct fee_index
   {
      share_type            _fees = 0;
      transaction_id_type   _trx;

      fee_index( share_type fees = 0, transaction_id_type trx = transaction_id_type() )
      :_fees(fees),_trx(trx){}

      friend bool operator == ( const fee_index& a, const fee_index& b )
      {
          return std::tie( a._fees, a._trx ) == std::tie( b._fees, b._trx );
      }

      friend bool operator < ( const fee_index& a, const fee_index& b )
      {
          // Reverse so that highest fee is placed first in sorted maps
          return std::tie( a._fees, a._trx ) > std::tie( b._fees, b._trx );
      }
   };

   namespace detail
   {
      class chain_database_impl
      {
         public:
            void                                        load_checkpoints( const fc::path& data_dir )const;
            bool                                        replay_required( const fc::path& data_dir );
            void                                        open_database( const fc::path& data_dir );
            void                                        clear_invalidation_of_future_blocks();
            digest_type                                 initialize_genesis( const optional<path>& genesis_file,
                                                                            const bool statistics_enabled );
            void                                        populate_indexes();

            std::pair<block_id_type, block_fork_data>   store_and_index( const block_id_type& id, const full_block& blk );

            void                                        clear_pending(  const full_block& block_data );
            void                                        revalidate_pending();

            void                                        switch_to_fork( const block_id_type& block_id );
            void                                        extend_chain( const full_block& blk );
            vector<block_id_type>                       get_fork_history( const block_id_type& id );
            void                                        pop_block();

            void                                        mark_invalid( const block_id_type& id, const fc::exception& reason );
            void                                        mark_as_unchecked( const block_id_type& id );
            void                                        mark_included( const block_id_type& id, bool state );

            std::vector<block_id_type>                  fetch_blocks_at_number( uint32_t block_num );

            std::pair<block_id_type, block_fork_data>   recursive_mark_as_linked( const std::unordered_set<block_id_type>& ids );
            void                                        recursive_mark_as_invalid( const std::unordered_set<block_id_type>& ids,
                                                                                   const fc::exception& reason );

            void                                        verify_header( const digest_block& block_digest,
                                                                       const public_key_type& block_signee )const;

            void                                        update_delegate_production_info( const block_header& block_header,
                                                                                         const block_id_type& block_id,
                                                                                         const public_key_type& block_signee,
                                                                                         const pending_chain_state_ptr& pending_state )const;

            void                                        pay_delegate( const block_id_type& block_id,
                                                                      const public_key_type& block_signee,
                                                                      const pending_chain_state_ptr& pending_state,
                                                                      oblock_record& block_record )const;

            void                                        execute_markets( const time_point_sec timestamp,
                                                                         const pending_chain_state_ptr& pending_state )const;

            void                                        apply_transactions( const full_block& block_data,
                                                                            const pending_chain_state_ptr& pending_state )const;

            void                                        update_active_delegate_list( const uint32_t block_num,
                                                                                     const pending_chain_state_ptr& pending_state )const;

            void                                        update_random_seed( const secret_hash_type& new_secret,
                                                                            const pending_chain_state_ptr& pending_state,
                                                                            oblock_record& block_record )const;

            void                                        save_undo_state( const uint32_t block_num,
                                                                         const block_id_type& block_id,
                                                                         const pending_chain_state_ptr& pending_state );

            void                                        update_head_block( const signed_block_header& block_header,
                                                                           const block_id_type& block_id );

            void                                        pay_delegate_v2( const block_id_type& block_id,
                                                                         const public_key_type& block_signee,
                                                                         const pending_chain_state_ptr& pending_state,
                                                                         oblock_record& block_record )const;

            void                                        pay_delegate_v1( const block_id_type& block_id,
                                                                         const public_key_type& block_signee,
                                                                         const pending_chain_state_ptr& pending_state,
                                                                         oblock_record& block_record )const;

            void                                        execute_markets_v1( const time_point_sec timestamp,
                                                                            const pending_chain_state_ptr& pending_state )const;

            void                                        update_active_delegate_list_v1( const uint32_t block_num,
                                                                                        const pending_chain_state_ptr& pending_state )const;

            void                                        debug_check_no_orders_overlap( const pending_chain_state_ptr& pending_state ) const;

            chain_database*                                                             self = nullptr;
            unordered_set<chain_observer*>                                              _observers;

            /* Transaction propagation */
            fc::future<void>                                                            _revalidate_pending;
            pending_chain_state_ptr                                                     _pending_trx_state = nullptr;
            bts::db::level_map<transaction_id_type, signed_transaction>                 _pending_transaction_db;
            map<fee_index, transaction_evaluation_state_ptr>                            _pending_fee_index;
            share_type                                                                  _relay_fee = BTS_BLOCKCHAIN_DEFAULT_RELAY_FEE;

            /* Block processing */
            uint32_t /* Only used to skip undo states when possible during replay */    _min_undo_block = 0;

            fc::mutex                                                                   _push_block_mutex;

            bts::db::level_map<block_id_type, full_block>                               _block_id_to_full_block;
            bts::db::fast_level_map<block_id_type, pending_chain_state>                 _block_id_to_undo_state;

            bts::db::level_map<uint32_t, vector<block_id_type>>                         _fork_number_db; // All siblings
            bts::db::level_map<block_id_type, block_fork_data>                          _fork_db;

            bts::db::level_map<block_id_type, int32_t>                                  _revalidatable_future_blocks_db; //int32_t is unused, this is a set

            bts::db::level_map<uint32_t, block_id_type>                                 _block_num_to_id_db; // Current chain

            bts::db::level_map<block_id_type, block_record>                             _block_id_to_block_record_db; // Statistics

            /* Current primary state */
            block_id_type                                                               _head_block_id;
            signed_block_header                                                         _head_block_header;

            bts::db::fast_level_map<uint8_t, property_record>                           _property_id_to_record;

            bts::db::fast_level_map<account_id_type, account_record>                    _account_id_to_record;
            bts::db::fast_level_map<string, account_id_type>                            _account_name_to_id;
            bts::db::fast_level_map<address, account_id_type>                           _account_address_to_id;
            set<vote_del>                                                               _delegate_votes;

            bts::db::fast_level_map<asset_id_type, asset_record>                        _asset_id_to_record;
            bts::db::fast_level_map<string, asset_id_type>                              _asset_symbol_to_id;

            bts::db::fast_level_map<slate_id_type, slate_record>                        _slate_id_to_record;

            bts::db::fast_level_map<balance_id_type, balance_record>                    _balance_id_to_record;

            bts::db::level_map<transaction_id_type, transaction_record>                 _transaction_id_to_record;
            set<unique_transaction_key>                                                 _unique_transactions;
            bts::db::level_map<address, unordered_set<transaction_id_type>>             _address_to_transaction_ids;

            bts::db::cached_level_map<burn_index, burn_record>                          _burn_index_to_record;

            bts::db::cached_level_map<status_index, status_record>                      _status_index_to_record;

            bts::db::cached_level_map<feed_index, feed_record>                          _feed_index_to_record;
            unordered_map<asset_id_type, unordered_map<account_id_type, feed_record>>   _nested_feed_map;

            bts::db::cached_level_map<market_index_key, order_record>                   _ask_db;
            bts::db::cached_level_map<market_index_key, order_record>                   _bid_db;

            bts::db::cached_level_map<market_index_key, order_record>                   _short_db;

            bts::db::cached_level_map<market_index_key, collateral_record>              _collateral_db;
            set<expiration_index>                                                       _collateral_expiration_index;

            bts::db::cached_level_map<uint32_t, vector<market_transaction>>             _market_transactions_db;
            bts::db::cached_level_map<market_history_key, market_history_record>        _market_history_db;

            bts::db::level_map<slot_index, slot_record>                                 _slot_index_to_record;
            bts::db::level_map<time_point_sec, account_id_type>                         _slot_timestamp_to_delegate;

            map<operation_type_enum, std::deque<operation>>                             _recent_operations;

            mutable fc::variants                                                        _debug_matching_error_log;
            mutable set<uint32_t>                                                       _debug_trap_blocks;
      };

  } // detail
} } // bts::blockchain

FC_REFLECT_TYPENAME( std::vector<bts::blockchain::block_id_type> )
FC_REFLECT_TYPENAME( std::unordered_set<bts::blockchain::transaction_id_type> )
FC_REFLECT( bts::blockchain::fee_index, (_fees)(_trx) )
