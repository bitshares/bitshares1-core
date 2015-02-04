#pragma once

#include <bts/blockchain/chain_database.hpp>
#include <bts/db/cached_level_map.hpp>
#include <bts/db/fast_level_map.hpp>
#include <fc/thread/mutex.hpp>

namespace bts { namespace blockchain {

   struct fee_index
   {
      fee_index( share_type fees = 0, transaction_id_type trx = transaction_id_type() )
      :_fees(fees),_trx(trx){}
      share_type          _fees;
      transaction_id_type _trx;
      friend bool operator == ( const fee_index& a, const fee_index& b )
      {
         return a._fees == b._fees && a._trx == b._trx;
      }
      friend bool operator < ( const fee_index& a, const fee_index& b )
      {
         if( a._fees == b._fees ) return a._trx < b._trx; /* Lowest id wins in ties */
         return a._fees > b._fees; /* Reverse so that highest fee is placed first in sorted maps */
      }
   };

   namespace detail
   {
      class chain_database_impl
      {
         public:
            void                                        load_checkpoints( const fc::path& data_dir )const;
            void                                        open_database(const fc::path& data_dir );
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


            fc::future<void> _revalidate_pending;
            fc::mutex        _push_block_mutex;

            /**
             *  Used to track the cumulative effect of all pending transactions that are known,
             *  new incomming transactions are evaluated relative to this state.
             *
             *  After a new block is pushed this state is recalculated based upon what ever
             *  pending transactions remain.
             */
            pending_chain_state_ptr                                                     _pending_trx_state = nullptr;

            chain_database*                                                             self = nullptr;
            unordered_set<chain_observer*>                                              _observers;
            share_type                                                                  _relay_fee = BTS_BLOCKCHAIN_DEFAULT_RELAY_FEE;

            bts::db::level_map<uint32_t, std::vector<block_id_type>>                    _fork_number_db;
            bts::db::level_map<block_id_type,block_fork_data>                           _fork_db;

            bts::db::level_map<block_id_type,int32_t>                                   _revalidatable_future_blocks_db; //int32_t is unused, this is a set

            bts::db::fast_level_map<block_id_type, pending_chain_state>                 _block_id_to_undo_state;

            // blocks in the current 'official' chain.
            bts::db::level_map<uint32_t,block_id_type>                                  _block_num_to_id_db;
            // all blocks from any fork..
            bts::db::level_map<block_id_type,block_record>                              _block_id_to_block_record_db;

            bts::db::level_map<block_id_type,full_block>                                _block_id_to_block_data_db;

            signed_block_header                                                         _head_block_header;
            block_id_type                                                               _head_block_id;

            bts::db::level_map<transaction_id_type, signed_transaction>                 _pending_transaction_db;
            map<fee_index, transaction_evaluation_state_ptr>                            _pending_fee_index;

            bts::db::fast_level_map<uint32_t, fc::variant>                              _property_db;

            bts::db::fast_level_map<account_id_type, account_record>                    _account_id_to_record;
            bts::db::fast_level_map<string, account_id_type>                            _account_name_to_id;
            bts::db::fast_level_map<address, account_id_type>                           _account_address_to_id;
            set<vote_del>                                                               _delegate_votes;

            bts::db::fast_level_map<asset_id_type, asset_record>                        _asset_id_to_record;
            bts::db::fast_level_map<string, asset_id_type>                              _asset_symbol_to_id;

            bts::db::fast_level_map<balance_id_type, balance_record>                    _balance_id_to_record;

            bts::db::level_map<transaction_id_type,transaction_record>                  _id_to_transaction_record_db;
            set<unique_transaction_key>                                                 _unique_transactions;

            bts::db::fast_level_map<slate_id_type, slate_record>                        _slate_id_to_record;

            bts::db::cached_level_map<feed_index, feed_record>                          _feed_index_to_record;
            unordered_map<asset_id_type, unordered_map<account_id_type, feed_record>>   _nested_feed_map;

            bts::db::cached_level_map<burn_record_key, burn_record_value>               _burn_db;

            bts::db::cached_level_map<market_index_key, order_record>                   _ask_db;
            bts::db::cached_level_map<market_index_key, order_record>                   _bid_db;
            bts::db::cached_level_map<market_index_key, order_record>                   _relative_ask_db;
            bts::db::cached_level_map<market_index_key, order_record>                   _relative_bid_db;

            bts::db::cached_level_map<market_index_key, order_record>                   _short_db; // interest,owner => order

            /** maintains a subset of _short_db that is currently at the price feed, sorted by interest rate, then owner
             *  Note: this index is read only outside of chain db
             **/
            set<market_index_key>                                                       _shorts_at_feed;  // cache all shorts currently at the feed

            /** maintains a sorted index of all shorts with a price limit by price limit, then interest, then owner */
            set< pair<price,market_index_key> >                                         _short_limit_index;

            bts::db::cached_level_map<market_index_key, collateral_record>              _collateral_db;     // expiration date == latest expiration
            set<expiration_index>                                                       _collateral_expiration_index;

            bts::db::cached_level_map<uint32_t, vector<market_transaction>>             _market_transactions_db;
            bts::db::cached_level_map<pair<asset_id_type,asset_id_type>, market_status> _market_status_db;
            bts::db::cached_level_map<market_history_key, market_history_record>        _market_history_db;

            bts::db::level_map<slot_index, slot_record>                                 _slot_index_to_record;
            bts::db::level_map<time_point_sec, account_id_type>                         _slot_timestamp_to_delegate;

            bts::db::level_map<object_id_type, object_record>                           _object_db;
            bts::db::level_map<edge_index_key, object_id_type/*edge id*/>               _edge_index;
            bts::db::level_map<edge_index_key, object_id_type/*edge id*/>               _reverse_edge_index;

            bts::db::level_map<string, site_record>                                     _site_index;

            /**
             *  This index is to facilitate light weight clients and is intended mostly for
             *  block explorers and other APIs serving data.
             */
            bts::db::level_map< pair<address,transaction_id_type>, int>                 _address_to_trx_index;

            bts::db::level_map<pair<asset_id_type,address>, object_id_type>             _auth_db;
            bts::db::level_map<pair<asset_id_type,proposal_id_type>, proposal_record>   _asset_proposal_db;

            map<operation_type_enum, std::deque<operation>>                             _recent_operations;
      };
  } // end namespace bts::blockchain::detail
} } // end namespace bts::blockchain

FC_REFLECT_TYPENAME( std::vector<bts::blockchain::block_id_type> )
FC_REFLECT( bts::blockchain::vote_del, (votes)(delegate_id) )
FC_REFLECT( bts::blockchain::fee_index, (_fees)(_trx) )
