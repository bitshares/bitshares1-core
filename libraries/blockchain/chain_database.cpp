#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/genesis_config.hpp>

#include <bts/db/level_pod_map.hpp>
#include <bts/db/level_map.hpp>

#include <fc/io/json.hpp>

#include <fc/log/logger.hpp>
using namespace bts::blockchain;


struct vote_del
{
   vote_del( int64_t v = 0, name_id_type del = 0 )
   :votes(v),delegate_id(del){}
   int64_t votes;
   name_id_type delegate_id;
   friend bool operator == ( const vote_del& a, const vote_del& b )
   {
      return a.votes == b.votes && a.delegate_id == b.delegate_id;
   }
   friend bool operator < ( const vote_del& a, const vote_del& b )
   {
      return a.votes > b.votes ? true : (a.votes == b.votes ? a.delegate_id > b.delegate_id : false);
   }
};
FC_REFLECT( vote_del, (votes)(delegate_id) )

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
      return a._fees > b._fees ? true : (a._fees == b._fees ? a._trx > b._trx : false);
   }
};
FC_REFLECT( fee_index, (_fees)(_trx) )

struct block_fork_data
{
   block_fork_data():is_linked(false),is_included(false){}

   bool invalid()const
   {
      if( !!is_valid ) return !*is_valid;
      return false;
   }
   bool valid()const
   {
      if( !!is_valid ) return *is_valid;
      return false;
   }
   bool can_link()const
   {
      return is_linked && !invalid();
   }

   std::vector<block_id_type> next_blocks; ///< IDs of all blocks that come after
   bool                       is_linked;   ///< is linked to genesis block

   /** if at any time this block was determiend to be valid or invalid then this
    * flag will be set.
    */
   fc::optional<bool>         is_valid;
   bool                       is_included; ///< is included in the current chain database
};
FC_REFLECT( block_fork_data, (next_blocks)(is_linked)(is_valid)(is_included) )
FC_REFLECT_TYPENAME( std::vector<bts::blockchain::block_id_type> )


namespace bts { namespace blockchain {

   namespace detail
   {

      class chain_database_impl
      {
         public:
            chain_database_impl():self(nullptr),_observer(nullptr),_last_asset_id(0),_last_name_id(0){}

            void                       initialize_genesis(fc::optional<fc::path> genesis_file = fc::optional<fc::path>());

            block_fork_data            store_and_index( const block_id_type& id, const full_block& blk );
            void                       clear_pending(  const full_block& blk );
            void                       switch_to_fork( const block_id_type& block_id );
            void                       extend_chain( const full_block& blk );
            std::vector<block_id_type> get_fork_history( const block_id_type& id );
            void                       pop_block();
            void                       mark_invalid( const block_id_type& id );
            void                       mark_included( const block_id_type& id, bool state );
            void                       verify_header( const full_block& );
            void                       apply_transactions( uint32_t block_num,
                                                           const std::vector<signed_transaction>&,
                                                           const pending_chain_state_ptr& );
            void                       pay_delegate( fc::time_point_sec time_slot, share_type amount,
                                                           const pending_chain_state_ptr& );
            void                       save_undo_state( const block_id_type& id,
                                                           const pending_chain_state_ptr& );
            void                       save_redo_state( const block_id_type& id,
                                                           const pending_chain_state_ptr& );
            void                       clear_redo_state( const block_id_type& id );
            void                       update_head_block( const full_block& blk );
            std::vector<block_id_type> fetch_blocks_at_number( uint32_t block_num );
            void                       recursive_mark_as_linked( const std::vector<block_id_type>& ids );

            void update_delegate_production_info( const full_block& block_data, 
                                                  const pending_chain_state_ptr& pending_state );

            chain_database*                                           self;
            chain_observer*                                           _observer;

            bts::db::level_map<uint32_t, std::vector<block_id_type> > _fork_number_db;
            bts::db::level_map<block_id_type,block_fork_data>         _fork_db;

            /** the data required to 'undo' the changes a block made to the database */
            bts::db::level_map<block_id_type,pending_chain_state>     _undo_state;
            bts::db::level_map<block_id_type,pending_chain_state>     _redo_state;

            // blocks in the current 'official' chain.
            bts::db::level_map<uint32_t,block_id_type>     _block_num_to_id;
            // all blocks from any fork..
            bts::db::level_map<block_id_type,full_block>   _block_id_to_block;

            // used to revert block state in the event of a fork
            // bts::db::level_map<uint32_t,undo_data>         _block_num_to_undo_data;

            signed_block_header _head_block_header;
            block_id_type       _head_block_id;

            bts::db::level_map< transaction_id_type, signed_transaction>         _pending_transactions;
            std::map< fee_index, transaction_evaluation_state_ptr >              _pending_fee_index;


            bts::db::level_map< asset_id_type, asset_record >                    _assets;
            bts::db::level_map< balance_id_type, balance_record >                _balances;
            bts::db::level_map< name_id_type, name_record >                      _names;

            bts::db::level_map< std::string, name_id_type >                      _name_index;
            bts::db::level_map< std::string, asset_id_type >                     _symbol_index;
            bts::db::level_pod_map< vote_del, int >                              _delegate_vote_index;

            /** used to prevent duplicate processing */
            bts::db::level_pod_map< transaction_id_type, transaction_location >  _processed_transaction_ids;

            asset_id_type _last_asset_id;
            name_id_type  _last_name_id;

            fc::ecc::public_key _trustee_key;
      };

      std::vector<block_id_type> chain_database_impl::fetch_blocks_at_number( uint32_t block_num )
      {
         std::vector<block_id_type> current_blocks;

         auto itr = _fork_number_db.find( block_num );
         if( itr.valid() ) return itr.value();
         return current_blocks;
      }

      void  chain_database_impl::clear_pending(  const full_block& blk )
      {
         std::unordered_set<transaction_id_type> confirmed_trx_ids;

         for( auto trx : blk.user_transactions )
         {
            auto id = trx.id();
            confirmed_trx_ids.insert( id );
            _pending_transactions.remove( id );
         }

         auto temp_pending_fee_index( _pending_fee_index );
         for( auto pair : temp_pending_fee_index )
         {
            auto fee_index = pair.first;

            if( confirmed_trx_ids.count( fee_index._trx ) > 0 )
               _pending_fee_index.erase( fee_index );
         }
      }

      void chain_database_impl::recursive_mark_as_linked( const std::vector<block_id_type>& ids )
      {
         std::vector<block_id_type> next_ids = ids;
         while( next_ids.size() )
         {
            std::vector<block_id_type> pending;
            for( auto item : next_ids )
            {
                block_fork_data record = _fork_db.fetch( item );
                record.is_linked = true;
                pending.insert( pending.end(), record.next_blocks.begin(), record.next_blocks.end() );
                _fork_db.store( item, record );
            }
            next_ids = pending;
         }
      }

      /**
       *  Place the block in the block tree, the block tree contains all blocks
       *  and tracks whether they are valid, linked, and current.
       *
       *  There are several options for this block:
       *
       *  1) It extends an existing block
       *      - a valid chain
       *      - an invalid chain
       *      - an unlinked chain
       *  2) It is free floating and doesn't link to anything we have
       *      - create two entries into the database
       *          - one for this block
       *          - placeholder for previous
       *      - mark both as unlinked
       *  3) It it provides the missing link between a the genesis block and existing chain
       *      - all next blocks need to be updated to change state to 'linked'
       */
      block_fork_data chain_database_impl::store_and_index( const block_id_type& block_id,
                                                            const full_block& block_data )
      { try {
          ilog( "block_number: ${n}   id: ${id}  prev: ${prev}",
                ("n",block_data.block_num)("id",block_id)("prev",block_data.previous) );

          // first of all store this block at the given block number
          _block_id_to_block.store( block_id, block_data );

          // update the parallel block list
          std::vector<block_id_type> parallel_blocks = fetch_blocks_at_number( block_data.block_num );
          std::find( parallel_blocks.begin(), parallel_blocks.end(), block_id );
          parallel_blocks.push_back( block_id );
          _fork_number_db.store( block_data.block_num, parallel_blocks );


          // now find how it links in.
          block_fork_data prev_fork_data;
          auto prev_itr = _fork_db.find( block_data.previous );
          if( prev_itr.valid() ) // we already know about its previous
          {
             ilog( "           we already know about its previous" );
             prev_fork_data = prev_itr.value();
             prev_fork_data.next_blocks.push_back(block_id);
             _fork_db.store( prev_itr.key(), prev_fork_data );
          }
          else
          {
             ilog( "           we don't know about its previous" );
             // create it... we do not know about the previous block so
             // we must create it and assume it is not linked...
             prev_fork_data.next_blocks.push_back(block_id);
             prev_fork_data.is_linked = block_data.previous == block_id_type(); //false;
             _fork_db.store( block_data.previous, prev_fork_data );
          }

          block_fork_data current_fork;
          auto cur_itr = _fork_db.find( block_id );
          if( cur_itr.valid() )
          {
             current_fork = cur_itr.value();
             ilog( "          current_fork: ${fork}", ("fork",current_fork) );
             if( !current_fork.is_linked && prev_fork_data.is_linked )
             {
                // we found the missing link
                current_fork.is_linked = true;
                recursive_mark_as_linked( current_fork.next_blocks );
             }
          }
          else
          {
             current_fork.is_linked = prev_fork_data.is_linked;
             ilog( "          current_fork: ${fork}", ("fork",current_fork) );
             _fork_db.store( block_id, current_fork );
          }
          return current_fork;
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }

      void chain_database_impl::mark_invalid( const block_id_type& block_id )
      {
         // fetch the fork data for block_id, mark it as invalid and
         // then mark every item after it as invalid as well.
      }
      void chain_database_impl::mark_included( const block_id_type& block_id, bool state )
      {
         // fetch the fork data for block_id, mark it as invalid and
         // then mark every item after it as invalid as well.
      }

      void chain_database_impl::switch_to_fork( const block_id_type& block_id )
      { try {
         ilog( "switch from fork ${id} to ${to_id}", ("id",_head_block_id)("to_id",block_id) );
         std::vector<block_id_type> history = get_fork_history( block_id );
         FC_ASSERT( history.size() > 0 );
         while( history.front() != _head_block_id )
         {
            pop_block();
         }
         for( uint32_t i = 1; i < history.size(); ++i )
         {
            extend_chain( self->get_block( history[i] ) );
         }
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }


      void chain_database_impl::apply_transactions( uint32_t block_num,
                                                    const std::vector<signed_transaction>& user_transactions,
                                                    const pending_chain_state_ptr& pending_state )
      {
         //ilog( "apply transactions ${block_num}", ("block_num",block_num) );
         uint32_t trx_num = 0;
         try {
            // apply changes from each transaction
            for( auto trx : user_transactions )
            {
               transaction_evaluation_state_ptr trx_eval_state =
                      std::make_shared<transaction_evaluation_state>(pending_state);
               trx_eval_state->evaluate( trx );
               //ilog( "evaluation: ${e}", ("e",*trx_eval_state) );
              // TODO:  capture the evaluation state with a callback for wallets...
              // summary.transaction_states.emplace_back( std::move(trx_eval_state) );

               transaction_location trx_loc( block_num, trx_num );
               //ilog( "store trx location: ${loc}", ("loc",trx_loc) );
               pending_state->store_transaction_location( trx.id(), trx_loc );
               ++trx_num;
            }
      } FC_RETHROW_EXCEPTIONS( warn, "", ("trx_num",trx_num) ) }

      void chain_database_impl::pay_delegate(  fc::time_point_sec time_slot, share_type amount, const pending_chain_state_ptr& pending_state)
      { try {
            auto delegate_record = pending_state->get_name_record( self->get_signing_delegate_id( time_slot ) );
            FC_ASSERT( !!delegate_record );
            FC_ASSERT( delegate_record->is_delegate() );
            delegate_record->delegate_info->pay_balance += amount;
            pending_state->store_name_record( *delegate_record );
      } FC_RETHROW_EXCEPTIONS( warn, "", ("time_slot",time_slot)("amount",amount) ) }

      void chain_database_impl::save_undo_state( const block_id_type& block_id,
                                               const pending_chain_state_ptr& pending_state )
      { try {
           pending_chain_state_ptr undo_state = std::make_shared<pending_chain_state>(nullptr);
           pending_state->get_undo_state( undo_state );
           _undo_state.store( block_id, *undo_state );
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }

      void chain_database_impl::save_redo_state( const block_id_type& block_id,
                                                 const pending_chain_state_ptr& pending_state )
      {try{
           _redo_state.store( block_id, *pending_state );
      }FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }
      void chain_database_impl::clear_redo_state( const block_id_type& id )
      {
         _redo_state.remove(id);
      }

      void chain_database_impl::verify_header( const full_block& block_data )
      { try {
            // validate preliminaries:
            FC_ASSERT( block_data.block_num == _head_block_header.block_num + 1 );
            FC_ASSERT( block_data.previous  == _head_block_id );
            FC_ASSERT( block_data.timestamp.sec_since_epoch() % BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC == 0 );
            FC_ASSERT( block_data.timestamp > _head_block_header.timestamp, "",
                       ("block_data.timestamp",block_data.timestamp)("timestamp()",_head_block_header.timestamp)  );
            fc::time_point_sec now = fc::time_point::now();
            FC_ASSERT( block_data.timestamp <=  now,
                       "${t} < ${now}", ("t",block_data.timestamp)("now",now));

            size_t block_size = block_data.block_size();
            auto   expected_next_fee = block_data.next_fee( self->get_fee_rate(),  block_size );

            FC_ASSERT( block_data.fee_rate  == expected_next_fee );

            digest_block digest_data(block_data);
            FC_ASSERT( digest_data.validate_digest() );
            FC_ASSERT( digest_data.validate_unique() );
            FC_ASSERT( block_data.validate_signee( self->get_signing_delegate_key(block_data.timestamp) ),
                       "", ("signing_delegate_key", self->get_signing_delegate_key(block_data.timestamp))
                           ("signing_delegate_id", self->get_signing_delegate_id( block_data.timestamp)) );
      } FC_RETHROW_EXCEPTIONS( warn, "" ) }

      void chain_database_impl::update_head_block( const full_block& block_data )
      {
         _head_block_header = block_data;
         _head_block_id = block_data.id();
      }

      /**
       *  A block should be produced every BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC. If we do not have
       *  a block for any multiple of this interval between block_data and the current head block then
       *  we need to lookup the delegates that should have produced a block at that interval and then
       *  increment their missed block count.
       *
       *  Then we need to increment the produced_block count for the delegate that produced block_data.
       *
       *  Note that the header of block_data has already been verified by the caller and that updates
       *  are applied to pending_state.
       */
      void chain_database_impl::update_delegate_production_info( const full_block& block_data,
                                                                 const pending_chain_state_ptr& pending_state )
      {
          auto timestamp = _head_block_header.timestamp;
          if( _head_block_header.block_num == 0 )
          {
              timestamp = block_data.timestamp;
              timestamp -= BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
          }

          do
          {
              timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;

              auto delegate_id = self->get_signing_delegate_id( timestamp );
              auto delegate_rec = pending_state->get_name_record( delegate_id );

              if( timestamp != block_data.timestamp )
                  delegate_rec->delegate_info->blocks_missed += 1;
              else
                  delegate_rec->delegate_info->blocks_produced += 1;

              pending_state->store_name_record( *delegate_rec );
          }
          while( timestamp != block_data.timestamp );
      }

      /**
       *  Performs all of the block validation steps and throws if error.
       */
      void chain_database_impl::extend_chain( const full_block& block_data )
      { try {
         auto block_id = block_data.id();
         try {
            verify_header( block_data );

            block_summary summary;
            summary.block_data = block_data;

            /* Create a pending state to track changes that would apply as we evaluate the block */
            pending_chain_state_ptr pending_state = std::make_shared<pending_chain_state>(self->shared_from_this());
            summary.applied_changes = pending_state;

            /** Increment the blocks produced or missed for all delegates. This must be done
             *  before applying transactions because it depends upon the current order.
             **/
            update_delegate_production_info( block_data, pending_state );

            // apply any deterministic operations such as market operations before we preterb indexes
            //apply_deterministic_updates(pending_state);

            //ilog( "block data: ${block_data}", ("block_data",block_data) );
            apply_transactions( block_data.block_num, block_data.user_transactions, pending_state );

            pay_delegate( block_data.timestamp, block_data.delegate_pay_rate, pending_state );


            save_undo_state( block_id, pending_state );

            // in case we crash durring apply changes... remember what we
            // were in the process of applying...
            //
            // TODO: what if we crash in the middle of save_redo_state?
            //
            // on launch if data in redo database is not marked as
            // included... the re-apply the redo state and mark it as
            // included.
            save_redo_state( block_id, pending_state );

            // TODO: verify that apply changes can be called any number of
            // times without changing the database other than the first
            // attempt.
            // ilog( "apply changes\n${s}", ("s",fc::json::to_pretty_string( *pending_state) ) );
            pending_state->apply_changes();

            mark_included( block_id, true );

            // we have compelted successfully (as far as we can tell,
            // we can free the redo state
            clear_redo_state( block_id );

            update_head_block( block_data );

            clear_pending( block_data );

            _block_num_to_id.store( block_data.block_num, block_id );
            if( _observer ) _observer->block_applied( summary );
         }
         catch ( const fc::exception& )
         {
            mark_invalid( block_id );
            throw;
         }
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block",block_data) ) }

      /**
       * Traverse the previous links of all blocks in fork until we find one that is_included
       *
       * The last item in the result will be the only block id that is already included in
       * the blockchain.  
       */
      std::vector<block_id_type> chain_database_impl::get_fork_history( const block_id_type& id )
      { try {
         ilog( "" );
         std::vector<block_id_type> history;
         history.push_back( id );

         block_id_type next_id = id;
         while( true )
         {
            auto header = self->get_block_header( next_id );
            ilog( "header: ${h}", ("h",header) );
            history.push_back( header.previous );
            if( header.previous == block_id_type() )
            {
               ilog( "return: ${h}", ("h",history) );
               return history;
            }
            auto prev_fork_data = _fork_db.fetch( header.previous );

            /// this shouldn't happen if the database invariants are properly maintained 
            FC_ASSERT( prev_fork_data.is_linked, "we hit a dead end, this fork isn't really linked!" );
            if( prev_fork_data.is_included )
            {
               ilog( "return: ${h}", ("h",history) );
               return history;
            }
            next_id = header.previous;
         }
         ilog( "${h}", ("h",history) );
         return history;
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",id) ) }

      void  chain_database_impl::pop_block()
      { try {
         // update the is_included flag on the fork data
         auto fork_data = _fork_db.fetch( _head_block_id );
         fork_data.is_included = false;
         _fork_db.store( _head_block_id, fork_data );
         // update the block_num_to_block_id index
         _block_num_to_id.remove( _head_block_header.block_num );

         // fetch the undo state for the head block
         auto undo_state = _undo_state.fetch( _head_block_id );
         undo_state.set_prev_state( self->shared_from_this() );
         undo_state.apply_changes();
      } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   } // namespace detail

   chain_database::chain_database()
   :my( new detail::chain_database_impl() )
   {
      my->self = this;
   }

   chain_database::~chain_database()
   {
      try {
         close();
      }
      catch ( const fc::exception& e )
      {
         wlog( "unexpected exception closing database\n ${e}", ("e",e.to_detail_string() ) );
      }
      catch ( ... )
      {
         wlog( "unexpected exception closing database\n" );
      }
   }
   std::vector<name_id_type> chain_database::get_active_delegates()const
   {
      return get_delegates_by_vote( 0, BTS_BLOCKCHAIN_NUM_DELEGATES );
   }

   /**
    *  @return the top BTS_BLOCKCHAIN_NUM_DELEGATES by vote
    */
   std::vector<name_id_type> chain_database::get_delegates_by_vote(uint32_t first, uint32_t count )const
   { try {
      auto del_vote_itr = my->_delegate_vote_index.begin();
      std::vector<name_id_type> sorted_delegates;
      uint32_t pos = 0;
      while( sorted_delegates.size() < count && del_vote_itr.valid() )
      {
         if( pos >= first )
            sorted_delegates.push_back( del_vote_itr.key().delegate_id );
         ++pos;
         ++del_vote_itr;
      }
      return sorted_delegates;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   /**
    *  @return the top BTS_BLOCKCHAIN_NUM_DELEGATES by vote
    */
   std::vector<name_record> chain_database::get_delegate_records_by_vote(uint32_t first, uint32_t count )const
   { try {
      auto del_vote_itr = my->_delegate_vote_index.begin();
      std::vector<name_record> sorted_delegates;
      uint32_t pos = 0;
      while( sorted_delegates.size() < count && del_vote_itr.valid() )
      {
         if( pos >= first )
            sorted_delegates.push_back( *get_name_record(del_vote_itr.value()) );
         ++pos;
         ++del_vote_itr;
      }
      return sorted_delegates;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void chain_database::open( const fc::path& data_dir, fc::optional<fc::path> genesis_file )
   { try {
      fc::create_directories( data_dir );

      my->_fork_number_db.open( data_dir / "fork_number_db", true );
      my->_fork_db.open( data_dir / "fork_db", true );
      my->_undo_state.open( data_dir / "undo_state", true );
      my->_redo_state.open( data_dir / "redo_state", true );

      my->_block_num_to_id.open( data_dir / "block_num_to_id", true );
      my->_pending_transactions.open( data_dir / "pending_transactions", true );
      my->_processed_transaction_ids.open( data_dir / "processed_transactions", true );
      my->_block_id_to_block.open( data_dir / "block_id_to_block", true );
      my->_assets.open( data_dir / "assets", true );
      my->_names.open( data_dir / "names", true );
      my->_balances.open( data_dir / "balances", true );

      my->_name_index.open( data_dir / "name_index", true );
      my->_symbol_index.open( data_dir / "symbol_index", true );
      my->_delegate_vote_index.open( data_dir / "delegate_vote_index", true );

      // TODO: check to see if we crashed durring the last write
      //   if so, then apply the last undo operation stored.

      uint32_t       last_block_num = -1;
      block_id_type  last_block_id;
      my->_block_num_to_id.last( last_block_num, last_block_id );
      if( last_block_num != uint32_t(-1) )
      {
         my->_head_block_header = get_block( last_block_id );
         my->_head_block_id = last_block_id;
         my->_names.fetch( my->_last_name_id );
         my->_assets.fetch( my->_last_asset_id );
      }

      //  process the pending transactions to cache by fees
      auto pending_itr = my->_pending_transactions.begin();
      while( pending_itr.valid() )
      {
         try {
            auto trx = pending_itr.value();
            auto trx_id = trx.id();
            auto eval_state = evaluate_transaction( trx );
            share_type fees = eval_state->get_fees();
            my->_pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
            my->_pending_transactions.store( trx_id, trx );
         }
         catch ( const fc::exception& e )
         {
            wlog( "error processing pending transaction: ${e}", ("e",e.to_detail_string() ) );
         }
         ++pending_itr;
      }

      if( last_block_num == uint32_t(-1) )
         my->initialize_genesis(genesis_file);

   } FC_RETHROW_EXCEPTIONS( warn, "", ("data_dir",data_dir) ) }

   void chain_database::close()
   { try {
      my->_pending_transactions.close();
      my->_processed_transaction_ids.close();
      my->_block_num_to_id.close();

      my->_block_num_to_id.close();
      my->_block_id_to_block.close();
      my->_assets.close();
      my->_names.close();
      my->_balances.close();

      my->_name_index.close();
      my->_symbol_index.close();
      my->_delegate_vote_index.close();

   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   name_id_type chain_database::get_signing_delegate_id( fc::time_point_sec sec )const
   { try {
      FC_ASSERT( sec >= my->_head_block_header.timestamp );

      uint64_t  interval_number = sec.sec_since_epoch() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      unsigned  delegate_pos = (unsigned)(interval_number % BTS_BLOCKCHAIN_NUM_DELEGATES);
      auto sorted_delegates = get_active_delegates();

      FC_ASSERT( delegate_pos < sorted_delegates.size() );
      return  sorted_delegates[delegate_pos];
   } FC_RETHROW_EXCEPTIONS( warn, "", ("sec",sec) ) }

   fc::ecc::public_key chain_database::get_signing_delegate_key( fc::time_point_sec sec )const
   { try {
      auto delegate_record = get_name_record( get_signing_delegate_id( sec ) );
      FC_ASSERT( !!delegate_record );
      return delegate_record->active_key;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("sec", sec) ) }

   transaction_evaluation_state_ptr chain_database::evaluate_transaction( const signed_transaction& trx )
   { try {
      pending_chain_state_ptr          pend_state = std::make_shared<pending_chain_state>(shared_from_this());
      transaction_evaluation_state_ptr trx_eval_state = std::make_shared<transaction_evaluation_state>(pend_state);

      trx_eval_state->evaluate( trx );

      return trx_eval_state;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx) ) }

   signed_block_header  chain_database::get_block_header( const block_id_type& block_id )const
   { try {
      return get_block( block_id );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }

   signed_block_header  chain_database::get_block_header( uint32_t block_num )const
   { try {
      return get_block( block_num );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("block_num",block_num) ) }

   full_block           chain_database::get_block( const block_id_type& block_id )const
   { try {
      return my->_block_id_to_block.fetch(block_id);
   } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }

   full_block           chain_database::get_block( uint32_t block_num )const
   { try {
      auto block_id = my->_block_num_to_id.fetch( block_num );
      return get_block( block_id );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("block_num",block_num) ) }

   signed_block_header  chain_database::get_head_block()const
   {
      return my->_head_block_header;
   }

   otransaction_location chain_database::get_transaction_location( const transaction_id_type& trx_id )const
   {
      otransaction_location oloc;
      auto loc_itr = my->_processed_transaction_ids.find( trx_id );
      if( loc_itr.valid() ) return loc_itr.value();
      return oloc;
   }

   /**
    *  Adds the block to the database and manages any reorganizations as a result.
    *
    */
   void chain_database::push_block( const full_block& block_data )
   { try {
      auto block_id        = block_data.id();
      auto current_head_id = my->_head_block_id;

      block_fork_data fork = my->store_and_index( block_id, block_data );

      //ilog( "previous ${p} ==? current ${c}", ("p",block_data.previous)("c",current_head_id) );
      if( block_data.previous == current_head_id )
      {
         // attempt to extend chain
         return my->extend_chain( block_data );
      }
      else if( fork.can_link() && block_data.block_num > my->_head_block_header.block_num )
      {
         try {
            my->switch_to_fork( block_id );
         }
         catch ( const fc::exception& e )
         {
            wlog( "attempt to switch to fork failed: ${e}, reverting", ("e",e.to_detail_string() ) );
            my->switch_to_fork( current_head_id );
         }
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("block",block_data) ) }




   /** return the timestamp from the head block */
   fc::time_point_sec   chain_database::now()const
   {
      return my->_head_block_header.timestamp;
   }

         /** return the current fee rate in millishares */
   int64_t              chain_database::get_fee_rate()const
   {
      return my->_head_block_header.fee_rate;
   }

   int64_t              chain_database::get_delegate_pay_rate()const
   {
      return my->_head_block_header.delegate_pay_rate;
   }


   oasset_record        chain_database::get_asset_record( asset_id_type id )const
   {
      auto itr = my->_assets.find( id );
      if( itr.valid() )
         return itr.value();
      return oasset_record();
   }

   obalance_record      chain_database::get_balance_record( const balance_id_type& balance_id )const
   {
      auto itr = my->_balances.find( balance_id );
      if( itr.valid() )
         return itr.value();
      return obalance_record();
   }

   oname_record         chain_database::get_name_record( name_id_type name_id )const
   {
      auto itr = my->_names.find( name_id );
      if( itr.valid() )
         return itr.value();
      return oname_record();
   }

   void      chain_database::remove_asset_record( asset_id_type asset_id )const
   { try {
      try {
         auto asset_rec = get_asset_record( asset_id );
         my->_assets.remove( asset_id );
         if( asset_rec )
            my->_symbol_index.remove( asset_rec->symbol );
      } catch ( const fc::exception& e )
      {
         wlog( "caught exception ${e}", ("e", e.to_detail_string() ) );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("asset_id",asset_id) ) }

   void      chain_database::remove_balance_record( const balance_id_type& balance_id )const
   { try {
      try {
         my->_balances.remove( balance_id );
      } catch ( const fc::exception& e )
      {
         wlog( "caught exception ${e}", ("e", e.to_detail_string() ) );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("balance_id",balance_id) ) }

   void      chain_database::remove_name_record( name_id_type name_id )const
   { try {
      try {
         auto name_rec = get_name_record( name_id );
         my->_names.remove( name_id );
         if( name_rec )
            my->_name_index.remove( name_rec->name );
         // TODO: remove vote index as well...
      } catch ( const fc::exception& e )
      {
         wlog( "caught exception ${e}", ("e", e.to_detail_string() ) );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name_id",name_id) ) }


   oasset_record        chain_database::get_asset_record( const std::string& symbol )const
   { try {
       auto symbol_id_itr = my->_symbol_index.find( symbol );
       if( symbol_id_itr.valid() )
          return get_asset_record( symbol_id_itr.value() );
       return oasset_record();
   } FC_RETHROW_EXCEPTIONS( warn, "", ("symbol",symbol) ) }

   oname_record         chain_database::get_name_record( const std::string& name )const
   { try {
       auto name_id_itr = my->_name_index.find( name );
       if( name_id_itr.valid() )
          return get_name_record( name_id_itr.value() );
       return oname_record();
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name) ) }


   void chain_database::store_asset_record( const asset_record& r )
   { try {
       my->_assets.store( r.id, r );
       my->_symbol_index.store( r.symbol, r.id );
       if( r.id > my->_last_asset_id ) my->_last_asset_id = r.id;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("record", r) ) }


   void chain_database::store_balance_record( const balance_record& r )
   { try {
       if( r.balance == 0 )
       {
          my->_balances.remove( r.id() );
       }
       else
       {
          my->_balances.store( r.id(), r );
       }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("record", r) ) }


   void chain_database::store_name_record( const name_record& r )
   { try {
       auto old_rec = get_name_record( r.id );
       my->_names.store( r.id, r );

       if( old_rec && old_rec->is_delegate() )
       {
          my->_delegate_vote_index.remove( vote_del( old_rec->net_votes(), r.id ) );
       }
       else // this is a new record...
       {
          my->_name_index.store( r.name, r.id );
       }

       if( r.is_delegate() )
          my->_delegate_vote_index.store( vote_del( r.net_votes(), r.id ),  0 );

       if( r.id > my->_last_name_id ) my->_last_name_id = r.id;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("record", r) ) }

   void  chain_database::store_transaction_location( const transaction_id_type& trx_id,
                                                     const transaction_location& loc )
   { try {
       my->_processed_transaction_ids.store( trx_id, loc );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("id", trx_id)("location",loc) ) }


   asset_id_type        chain_database::last_asset_id()const
   {
      return my->_last_asset_id;
   }

   asset_id_type        chain_database::new_asset_id()
   {
      return ++my->_last_asset_id;
   }


   name_id_type         chain_database::last_name_id()const
   {
      return my->_last_name_id;
   }

   name_id_type         chain_database::new_name_id()
   {
      return ++my->_last_name_id;
   }


   osigned_transaction chain_database::get_transaction( const transaction_id_type& trx_id )const
   { try {

      auto trx_loc = get_transaction_location( trx_id );
      ilog( "block_number: ${trx_loc}", ("trx_loc",trx_loc) );
      if( !trx_loc ) return osigned_transaction();
      auto block_id = my->_block_num_to_id.fetch( trx_loc->block_num );
      auto block_data = my->_block_id_to_block.fetch( block_id );
      FC_ASSERT( block_data.user_transactions.size() > trx_loc->trx_num );

      return block_data.user_transactions[ trx_loc->trx_num ];
   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx_id",trx_id) ) }

   void    chain_database::scan_assets( const std::function<void( const asset_record& )>& callback )
   {
        auto asset_itr = my->_assets.begin();
        while( asset_itr.valid() )
        {
           callback( asset_itr.value() );
           ++asset_itr;
        }
   }

   void    chain_database::scan_balances( const std::function<void( const balance_record& )>& callback )
   {
        auto balances = my->_balances.begin();
        while( balances.valid() )
        {
           callback( balances.value() );
           ++balances;
        }
   }
   void    chain_database::scan_names( const std::function<void( const name_record& )>& callback )
   {
        auto name_itr = my->_names.begin();
        while( name_itr.valid() )
        {
           callback( name_itr.value() );
           ++name_itr;
        }
   }

   /** this should throw if the trx is invalid */
   transaction_evaluation_state_ptr chain_database::store_pending_transaction( const signed_transaction& trx )
   { try {
      auto trx_id = trx.id();
      auto current_itr = my->_pending_transactions.find( trx_id );
      if( current_itr.valid() ) return nullptr;

      auto eval_state = evaluate_transaction( trx );
      share_type fees = eval_state->get_fees();
      my->_pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
      my->_pending_transactions.store( trx_id, trx );

      return eval_state;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx) ) }

   /** returns all transactions that are valid (indepdnent of eachother) sorted by fee */
   std::vector<transaction_evaluation_state_ptr> chain_database::get_pending_transactions()const
   {
      std::vector<transaction_evaluation_state_ptr> trxs;
      for( auto item : my->_pending_fee_index )
      {
          trxs.push_back( item.second );
      }
      return trxs;
   }
   bool chain_database::is_known_transaction( const transaction_id_type& trx_id )
   {
      auto pending_itr = my->_pending_transactions.find( trx_id );
      if( pending_itr.valid() ) return true;
      return !!get_transaction_location( trx_id );
   }

   full_block chain_database::generate_block( fc::time_point_sec timestamp )
   {
      full_block next_block;

      pending_chain_state_ptr pending_state = std::make_shared<pending_chain_state>(shared_from_this());
      auto pending_trx = get_pending_transactions();

      size_t block_size = 0;
      share_type total_fees = 0;
      for( auto item : pending_trx )
      {
         auto trx_size = item->trx.data_size();
         if( block_size + trx_size > BTS_BLOCKCHAIN_MAX_BLOCK_SIZE )
            break;
         block_size += trx_size;
         // make modifications to tempoary state...
         pending_chain_state_ptr pending_trx_state = std::make_shared<pending_chain_state>(pending_state);
         transaction_evaluation_state_ptr trx_eval_state = std::make_shared<transaction_evaluation_state>(pending_trx_state);
         try {
            trx_eval_state->evaluate( item->trx );
            // TODO: what about fees in other currencies?
            total_fees += trx_eval_state->get_fees(0);
            // apply temporary state to block state
            pending_trx_state->apply_changes();
            next_block.user_transactions.push_back( item->trx );
         }
         catch ( const fc::exception& e )
         {
            wlog( "pending transaction was found to be invalid in context of block\n ${trx} \n${e}",
                  ("trx",fc::json::to_pretty_string(item->trx) )("e",e.to_detail_string()) );
         }
      }

      next_block.block_num          = my->_head_block_header.block_num + 1;
      next_block.previous           = my->_head_block_id;
      next_block.timestamp          = timestamp;
      next_block.fee_rate           = next_block.next_fee( my->_head_block_header.fee_rate, block_size );
      next_block.transaction_digest = digest_block(next_block).calculate_transaction_digest();
      next_block.delegate_pay_rate  = next_block.next_delegate_pay( my->_head_block_header.delegate_pay_rate, total_fees );

    //  elog( "initial pay rate: ${R}   total fees: ${F} next: ${N}",
    //        ( "R", my->_head_block_header.delegate_pay_rate )( "F", total_fees )("N",next_block.delegate_pay_rate) );

      return next_block;
   }

   void detail::chain_database_impl::initialize_genesis(fc::optional<fc::path> genesis_file)
   {
      #include "genesis.json"

      genesis_block_config config;
      if (genesis_file)
        config = fc::json::from_file(*genesis_file).as<genesis_block_config>();
      else
        config = fc::json::from_string( genesis_json ).as<genesis_block_config>();

      double total_unscaled = 0;
      for( auto item : config.balances ) total_unscaled += item.second;
      double scale_factor = BTS_BLOCKCHAIN_INITIAL_SHARES / total_unscaled;

      std::vector<name_config> delegate_config;
      for( auto item : config.names )
         if( item.is_delegate ) delegate_config.push_back( item );

      FC_ASSERT( delegate_config.size() >= BTS_BLOCKCHAIN_NUM_DELEGATES,
                 "genesis.json does not contain enough initial delegates",
                 ("required",BTS_BLOCKCHAIN_NUM_DELEGATES)("provided",delegate_config.size()) );

      // everyone will vote for every delegate initially
      scale_factor /= BTS_BLOCKCHAIN_NUM_DELEGATES;

      name_record god; god.id = 0; god.name = "god";
      self->store_name_record( god );

      asset_record base_asset;
      base_asset.id = 0;
      base_asset.symbol = BTS_ADDRESS_PREFIX;
      base_asset.name = "BitShares XTS";
      base_asset.description = "Shares in the DAC";
      base_asset.issuer_name_id = god.id;
      base_asset.current_share_supply = BTS_BLOCKCHAIN_INITIAL_SHARES;
      base_asset.maximum_share_supply = BTS_BLOCKCHAIN_INITIAL_SHARES;
      base_asset.collected_fees = 0;
      self->store_asset_record( base_asset );

      fc::time_point_sec timestamp = config.timestamp;
      int32_t i = 1;
      for( auto name : config.names )
      {
         name_record rec;
         rec.id                = i;
         rec.name              = name.name;
         rec.owner_key         = name.owner;
         rec.active_key        = name.owner;
         rec.registration_date = timestamp;
         rec.last_update       = timestamp;
         if( name.is_delegate )
         {
            rec.delegate_info = delegate_stats();
            rec.delegate_info->votes_for  = BTS_BLOCKCHAIN_INITIAL_SHARES/delegate_config.size();
         }
         self->store_name_record( rec );
         ++i;
      }

      for( auto item : config.balances )
      {
         for( uint32_t delegate_id = 1; delegate_id <= BTS_BLOCKCHAIN_NUM_DELEGATES; ++delegate_id )
         {
            balance_record initial_balance( item.first,
                                            asset( share_type( item.second * scale_factor), 0 ), delegate_id );
            self->store_balance_record( initial_balance );
         }
      }
   }

   void chain_database::set_observer( chain_observer* observer )
   {
      my->_observer = observer;
   }
   bool chain_database::is_known_block( const block_id_type& block_id )const
   {
      auto itr = my->_block_id_to_block.find( block_id );
      return itr.valid();
   }
   uint32_t chain_database::get_block_num( const block_id_type& block_id )const
   { try {
      if( block_id == block_id_type() )
         return 0;
      auto itr = my->_block_id_to_block.find( block_id );
      FC_ASSERT( itr.valid() );
      return itr.value().block_num;
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to find block ${block_id}", ("block_id", block_id) ) }

    uint32_t         chain_database::get_head_block_num()const
    {
       return my->_head_block_header.block_num;
    }

    block_id_type      chain_database::get_head_block_id()const
    {
       return my->_head_block_id;
    }
    std::vector<name_record> chain_database::get_names( const std::string& first, uint32_t count )const
    { try {
       auto itr = my->_name_index.lower_bound(first);
       std::vector<name_record> names;
       while( itr.valid() && names.size() < count )
       {
          names.push_back( *get_name_record( itr.value() ) );
          ++itr;
       }
       return names;
    } FC_RETHROW_EXCEPTIONS( warn, "", ("first",first)("count",count) )  }

} } // namespace bts::blockchain
