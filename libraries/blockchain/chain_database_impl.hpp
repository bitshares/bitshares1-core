#pragma once
//#define DEFAULT_LOGGER "blockchain"

#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/checkpoints.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/genesis_config.hpp>
#include <bts/blockchain/genesis_json.hpp>
#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/operation_factory.hpp>
#include <bts/blockchain/time.hpp>

#include <bts/db/cached_level_map.hpp>
#include <bts/db/level_map.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>
#include <fc/thread/unique_lock.hpp>

#include <algorithm>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace bts { namespace blockchain {

   struct vote_del
   {
      vote_del( int64_t v = 0, account_id_type del = 0 )
      :votes(v),delegate_id(del){}
      int64_t votes;
      account_id_type delegate_id;
      friend bool operator == ( const vote_del& a, const vote_del& b )
      {
         return a.votes == b.votes && a.delegate_id == b.delegate_id;
      }
      friend bool operator < ( const vote_del& a, const vote_del& b )
      {
         if( a.votes != b.votes ) return a.votes > b.votes; /* Reverse so maps sort in descending order */
         return a.delegate_id < b.delegate_id; /* Lowest id wins in ties */
      }
   };

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
            void                                        open_database(const fc::path& data_dir );
            digest_type                                 initialize_genesis( const optional<path>& genesis_file, bool chain_id_only = false );

            std::pair<block_id_type, block_fork_data>   store_and_index( const block_id_type& id, const full_block& blk );
            void                                        clear_pending(  const full_block& blk );
            void                                        switch_to_fork( const block_id_type& block_id );
            void                                        extend_chain( const full_block& blk );
            vector<block_id_type>                       get_fork_history( const block_id_type& id );
            void                                        pop_block();
            void                                        mark_invalid( const block_id_type& id, const fc::exception& reason );
            void                                        mark_included( const block_id_type& id, bool state );
            void                                        verify_header( const full_block&, const public_key_type& block_signee );
            void                                        apply_transactions( const full_block& block,
                                                                            const pending_chain_state_ptr& );
            void                                        pay_delegate( const block_id_type& block_id,
                                                                      const pending_chain_state_ptr&,
                                                                      const public_key_type& block_signee );
            void                                        save_undo_state( const block_id_type& id,
                                                                         const pending_chain_state_ptr& );
            void                                        update_head_block( const full_block& blk );
            std::vector<block_id_type>                  fetch_blocks_at_number( uint32_t block_num );
            std::pair<block_id_type, block_fork_data>   recursive_mark_as_linked( const std::unordered_set<block_id_type>& ids );
            void                                        recursive_mark_as_invalid( const std::unordered_set<block_id_type>& ids, const fc::exception& reason );

            void                                        execute_markets(const fc::time_point_sec& timestamp, const pending_chain_state_ptr& pending_state );
            void                                        update_random_seed( const secret_hash_type& new_secret,
                                                                            const pending_chain_state_ptr& pending_state );
            void                                        update_active_delegate_list(const full_block& block_data,
                                                                                    const pending_chain_state_ptr& pending_state );

            void                                        update_delegate_production_info( const full_block& block_data,
                                                                                         const pending_chain_state_ptr& pending_state,
                                                                                         const public_key_type& block_signee );

            void                                        revalidate_pending();

            fc::future<void> _revalidate_pending;
            fc::mutex        _push_block_mutex;

            /**
             *  Used to track the cumulative effect of all pending transactions that are known,
             *  new incomming transactions are evaluated relative to this state.
             *
             *  After a new block is pushed this state is recalculated based upon what ever
             *  pending transactions remain.
             */
            pending_chain_state_ptr                                                     _pending_trx_state;

            chain_database*                                                             self = nullptr;
            unordered_set<chain_observer*>                                              _observers;
            digest_type                                                                 _chain_id;
            bool                                                                        _skip_signature_verification;
            share_type                                                                  _relay_fee;

            bts::db::cached_level_map<uint32_t, std::vector<market_transaction>>        _market_transactions_db;
            bts::db::level_map<slate_id_type, delegate_slate>                           _slate_db;
            bts::db::level_map<uint32_t, std::vector<block_id_type>>                    _fork_number_db;
            bts::db::level_map<block_id_type,block_fork_data>                           _fork_db;
            bts::db::cached_level_map<uint32_t, fc::variant>                            _property_db;
#if 0
            bts::db::level_map<proposal_id_type, proposal_record>                       _proposal_db;
            bts::db::level_map<proposal_vote_id_type, proposal_vote>                    _proposal_vote_db;
#endif

            /** the data required to 'undo' the changes a block made to the database */
            bts::db::level_map<block_id_type,pending_chain_state>                       _undo_state_db;

            // blocks in the current 'official' chain.
            bts::db::level_map<uint32_t,block_id_type>                                  _block_num_to_id_db;
            // all blocks from any fork..
            bts::db::level_map<block_id_type,block_record>                              _block_id_to_block_record_db;

            bts::db::level_map<block_id_type,full_block>                                _block_id_to_block_data_db;

            std::unordered_set<transaction_id_type>                                     _known_transactions;
            bts::db::level_map<transaction_id_type,transaction_record>                  _id_to_transaction_record_db;

            signed_block_header                                                         _head_block_header;
            block_id_type                                                               _head_block_id;

            bts::db::level_map<transaction_id_type, signed_transaction>                 _pending_transaction_db;
            std::map<fee_index, transaction_evaluation_state_ptr>                       _pending_fee_index;

            bts::db::level_map<asset_id_type, asset_record>                             _asset_db;
            bts::db::level_map<balance_id_type, balance_record>                         _balance_db;

            bts::db::level_map<burn_record_key, burn_record_value>                      _burn_db;

            bts::db::cached_level_map<account_id_type, account_record>                  _account_db;
            bts::db::cached_level_map<address, account_id_type>                         _address_to_account_db;

            bts::db::cached_level_map<string, account_id_type>                          _account_index_db;
            bts::db::level_map<string, asset_id_type>                                   _symbol_index_db;
            bts::db::cached_level_map<vote_del, int>                                    _delegate_vote_index_db;

            bts::db::level_map<time_point_sec, slot_record>                             _slot_record_db;

            bts::db::cached_level_map<market_index_key, order_record>                   _ask_db;
            bts::db::cached_level_map<market_index_key, order_record>                   _bid_db;
            bts::db::cached_level_map<market_index_key, order_record>                   _short_db;
            bts::db::cached_level_map<market_index_key, collateral_record>              _collateral_db;
            bts::db::cached_level_map<feed_index, feed_record>                          _feed_db;

            bts::db::level_map<std::pair<asset_id_type,asset_id_type>, market_status>   _market_status_db;
            bts::db::level_map<market_history_key, market_history_record>               _market_history_db;

            std::map<operation_type_enum, std::deque<operation>>                        _recent_operations;
      };
  } // end namespace bts::blockchain::detail
} } // end namespace bts::blockchain

FC_REFLECT_TYPENAME( std::vector<bts::blockchain::block_id_type> )
FC_REFLECT( bts::blockchain::vote_del, (votes)(delegate_id) )
FC_REFLECT( bts::blockchain::fee_index, (_fees)(_trx) )
