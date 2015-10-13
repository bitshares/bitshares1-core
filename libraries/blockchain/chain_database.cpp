#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/chain_database_impl.hpp>
#include <bts/blockchain/checkpoints.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/genesis_state.hpp>
#include <bts/blockchain/genesis_json.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/snapshot_state.hpp>
#include <bts/blockchain/time.hpp>

#include <fc/io/fstream.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>
#include <fc/thread/unique_lock.hpp>

#include <iomanip>
#include <iostream>

#ifndef WIN32
#include <csignal>
#endif

#include <bts/blockchain/fork_blocks.hpp>
#include <fc/real128.hpp>

namespace bts { namespace blockchain {

   const static short MAX_RECENT_OPERATIONS = 20;

   namespace detail
   {
      void chain_database_impl::revalidate_pending()
      {
            _pending_fee_index.clear();

            vector<transaction_id_type> trx_to_discard;

            _pending_trx_state = std::make_shared<pending_chain_state>( self->shared_from_this() );
            unsigned num_pending_transaction_considered = 0;
            auto itr = _pending_transaction_db.begin();
            while( itr.valid() )
            {
                signed_transaction trx = itr.value();
                const transaction_id_type trx_id = itr.key();
                assert(trx_id == trx.id());
                try
                {
                  transaction_evaluation_state_ptr eval_state = self->evaluate_transaction( trx, _relay_fee );
                  const share_type fees = eval_state->total_base_equivalent_fees_paid;
                  _pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
                  ilog( "revalidated pending transaction id ${id}", ("id", trx_id) );
                }
                catch ( const fc::canceled_exception& )
                {
                  throw;
                }
                catch ( const fc::exception& e )
                {
                  trx_to_discard.push_back(trx_id);
                  ilog( "discarding invalid transaction: ${id} ${e}",
                        ("id",trx_id)("e",e.to_detail_string()) );
                }
                ++num_pending_transaction_considered;
                ++itr;
            }

            for( const auto& item : trx_to_discard )
                _pending_transaction_db.remove( item );
            ilog("revalidate_pending complete, there are now ${pending_count} evaluated transactions, ${num_pending_transaction_considered} raw transactions",
                 ("pending_count", _pending_fee_index.size())
                 ("num_pending_transaction_considered", num_pending_transaction_considered));
      }

      void chain_database_impl::load_checkpoints( const fc::path& data_dir )const
      { try {
          for( const auto& item : CHECKPOINT_BLOCKS )
              LAST_CHECKPOINT_BLOCK_NUM = std::max( item.first, LAST_CHECKPOINT_BLOCK_NUM );

          const fc::path checkpoint_file = data_dir / "checkpoints.json";

          decltype( CHECKPOINT_BLOCKS ) external_checkpoints;
          fc::oexception file_exception;
          if( fc::exists( checkpoint_file ) )
          {
              try
              {
                  external_checkpoints = fc::json::from_file( checkpoint_file ).as<decltype( external_checkpoints )>();
              }
              catch( const fc::exception& e )
              {
                  file_exception = e;
              }
          }

          uint32_t external_checkpoint_max_block_num = 0;
          for( const auto& item : external_checkpoints )
              external_checkpoint_max_block_num = std::max( item.first, external_checkpoint_max_block_num );

          if( external_checkpoint_max_block_num >= LAST_CHECKPOINT_BLOCK_NUM )
          {
              ulog( "Using blockchain checkpoints from file: ${x}", ("x",checkpoint_file.preferred_string()) );
              CHECKPOINT_BLOCKS = external_checkpoints;
              LAST_CHECKPOINT_BLOCK_NUM = external_checkpoint_max_block_num;
              return;
          }

          if( !file_exception.valid() )
          {
              fc::remove_all( checkpoint_file );
              fc::json::save_to_file( CHECKPOINT_BLOCKS, checkpoint_file );
          }
          else
          {
              ulog( "Error loading blockchain checkpoints from file: ${x}", ("x",checkpoint_file.preferred_string()) );
          }

          if( !CHECKPOINT_BLOCKS.empty() )
              ulog( "Using built-in blockchain checkpoints" );
      } FC_CAPTURE_AND_RETHROW( (data_dir) ) }

      bool chain_database_impl::replay_required( const fc::path& data_dir )
      { try {
          _property_id_to_record.open( data_dir / "index/property_id_to_record" );
          const oproperty_record version_record = self->get_property_record( property_id_type::database_version );
          _property_id_to_record.close();

          if( !version_record.valid() || version_record->value.as_uint64() != BTS_BLOCKCHAIN_DATABASE_VERSION )
              return true;

          _block_num_to_id_db.open( data_dir / "raw_chain/block_num_to_id_db" );
          for( const auto& item : CHECKPOINT_BLOCKS )
          {
              const uint32_t block_num = item.first;
              const block_id_type& expected_block_id = item.second;

              const optional<block_id_type> actual_block_id = _block_num_to_id_db.fetch_optional( block_num );
              if( actual_block_id.valid() && *actual_block_id != expected_block_id )
              {
                  _block_num_to_id_db.close();
                  return true;
              }
          }
          _block_num_to_id_db.close();

          return false;
      } FC_CAPTURE_AND_RETHROW( (data_dir) ) }

      void chain_database_impl::open_database( const fc::path& data_dir )
      { try {
          _block_id_to_full_block.open( data_dir / "raw_chain/block_id_to_block_data_db" );
          _block_id_to_undo_state.open( data_dir / "index/block_id_to_undo_state" );

          _fork_number_db.open( data_dir / "index/fork_number_db" );
          _fork_db.open( data_dir / "index/fork_db" );

          _revalidatable_future_blocks_db.open( data_dir / "index/future_blocks_db" );

          _block_num_to_id_db.open( data_dir / "raw_chain/block_num_to_id_db" );

          _block_id_to_block_record_db.open( data_dir / "index/block_id_to_block_record_db" );

          _property_id_to_record.open( data_dir / "index/property_id_to_record" );

          _account_id_to_record.open( data_dir / "index/account_id_to_record" );
          _account_name_to_id.open( data_dir / "index/account_name_to_id" );
          _account_address_to_id.open( data_dir / "index/account_address_to_id" );

          _asset_id_to_record.open( data_dir / "index/asset_id_to_record" );
          _asset_symbol_to_id.open( data_dir / "index/asset_symbol_to_id" );

          _slate_id_to_record.open( data_dir / "index/slate_id_to_record" );

          _balance_id_to_record.open( data_dir / "index/balance_id_to_record" );

          _transaction_id_to_record.open( data_dir / "index/transaction_id_to_record" );
          _address_to_transaction_ids.open( data_dir / "index/address_to_transaction_ids" );

          _burn_index_to_record.open( data_dir / "index/burn_index_to_record" );

          _status_index_to_record.open( data_dir / "index/status_index_to_record" );

          _feed_index_to_record.open( data_dir / "index/feed_index_to_record" );

          _market_transactions_db.open( data_dir / "index/market_transactions_db" );

          _pending_transaction_db.open( data_dir / "index/pending_transaction_db" );

          _ask_db.open( data_dir / "index/ask_db" );
          _bid_db.open( data_dir / "index/bid_db" );
          _short_db.open( data_dir / "index/short_db" );
          _collateral_db.open( data_dir / "index/collateral_db" );

          _market_history_db.open( data_dir / "index/market_history_db" );

          _slot_index_to_record.open( data_dir / "index/slot_index_to_record" );
          _slot_timestamp_to_delegate.open( data_dir / "index/slot_timestamp_to_delegate" );

          _pending_trx_state = std::make_shared<pending_chain_state>( self->shared_from_this() );

          clear_invalidation_of_future_blocks();
      } FC_CAPTURE_AND_RETHROW( (data_dir) ) }

      void chain_database_impl::populate_indexes()
      { try {
          for( auto iter = _account_id_to_record.unordered_begin();
               iter != _account_id_to_record.unordered_end(); ++iter )
          {
              const account_record& record = iter->second;
              if( !record.is_retracted() && record.is_delegate() )
                  _delegate_votes.emplace( record.net_votes(), record.id );
          }

          for( auto iter = _transaction_id_to_record.begin(); iter.valid(); ++iter )
          {
              const transaction& trx = iter.value().trx;
              if( trx.expiration > self->now() )
                  _unique_transactions.emplace( trx, self->get_chain_id() );
          }

          for( auto iter = _feed_index_to_record.begin(); iter.valid(); ++iter )
          {
              const feed_index& index = iter.key();
              _nested_feed_map[ index.quote_id ][ index.delegate_id ] = iter.value();
          }

          for( auto iter = _collateral_db.begin(); iter.valid(); ++iter )
          {
              const market_index_key& key = iter.key();
              const collateral_record& record = iter.value();
              const expiration_index index{ key.order_price.quote_asset_id, record.expiration, key };
              _collateral_expiration_index.insert( index );
          }
      } FC_CAPTURE_AND_RETHROW() }

      void chain_database_impl::clear_invalidation_of_future_blocks()
      {
        for (auto block_id_itr = _revalidatable_future_blocks_db.begin(); block_id_itr.valid(); ++block_id_itr)
        {
          mark_as_unchecked( block_id_itr.key() );
        }
      }

      digest_type chain_database_impl::initialize_genesis( const optional<path>& genesis_file, const bool statistics_enabled )
      { try {
         genesis_state config;
         digest_type chain_id;

         if( !genesis_file.valid() )
         {
           std::cout << "Initializing state from built-in genesis file\n";
           config = get_builtin_genesis_block_config();
           chain_id = get_builtin_genesis_block_state_hash();
         }
         else
         {
           std::cout << "Initializing state from genesis file: "<< genesis_file->generic_string() << "\n";
           FC_ASSERT( fc::exists( *genesis_file ), "Genesis file '${file}' was not found.", ("file", *genesis_file) );

           if( genesis_file->extension() == ".json" )
           {
              config = fc::json::from_file(*genesis_file).as<genesis_state>();
           }
           else if( genesis_file->extension() == ".dat" )
           {
              fc::ifstream in( *genesis_file );
              fc::raw::unpack( in, config );
           }
           else
           {
              FC_ASSERT( false, "Invalid genesis format '${format}'", ("format",genesis_file->extension() ) );
           }
           fc::sha256::encoder enc;
           fc::raw::pack( enc, config );
           chain_id = enc.result();
         }

         if( chain_id == BTS_EXPECTED_CHAIN_ID )
             chain_id = BTS_DESIRED_CHAIN_ID;

         self->set_chain_id( chain_id );

         // Check genesis state
         FC_ASSERT( config.delegates.size() >= BTS_BLOCKCHAIN_NUM_DELEGATES,
                    "genesis.json does not contain enough initial delegates!",
                    ("required",BTS_BLOCKCHAIN_NUM_DELEGATES)("provided",config.delegates.size()) );

         const fc::time_point_sec timestamp = config.timestamp;

         // Initialize delegates
         account_id_type account_id = 0;
         for( const genesis_delegate& delegate : config.delegates )
         {
             ++account_id;
             account_record rec;
             rec.id = account_id;
             rec.name = delegate.name;
             rec.owner_key = delegate.owner;
             rec.set_active_key( timestamp, delegate.owner );
             rec.registration_date = timestamp;
             rec.last_update = timestamp;
             rec.delegate_info = delegate_stats();
             rec.delegate_info->pay_rate = 100;
             rec.set_signing_key( 0, delegate.owner );
             rec.meta_data = account_meta_info( titan_account );
             self->store_account_record( rec );
         }

         // For loading balances originally snapshotted from other chains
         const auto convert_raw_address = []( const string& raw_address ) -> address
         {
             static const vector<string> bts_prefixes{ "BTS", "KEY", "DVS", "XTS" };
             try
             {
                 return address( pts_address( raw_address ) );
             }
             catch( const fc::exception& )
             {
                 for( const string& prefix : bts_prefixes )
                 {
                     if( raw_address.find( prefix ) == 0 )
                     {
                         return address( BTS_ADDRESS_PREFIX + raw_address.substr( prefix.size() ) );
                     }
                 }
             }
             FC_THROW_EXCEPTION( invalid_pts_address, "Invalid raw address format!", ("raw_address",raw_address) );
         };

         // Initialize signature balances
         share_type total_base_supply = 0;
         for( const auto& genesis_balance : config.initial_balances )
         {
            const auto addr = convert_raw_address( genesis_balance.raw_address );
            balance_record initial_balance( addr, asset( genesis_balance.balance, 0 ), 0 );

            /* In case of redundant balances */
            const auto cur = self->get_balance_record( initial_balance.id() );
            if( cur.valid() ) initial_balance.balance += cur->balance;

            initial_balance.snapshot_info = snapshot_record( genesis_balance.raw_address, genesis_balance.balance );
            initial_balance.last_update = config.timestamp;
            self->store_balance_record( initial_balance );

            total_base_supply += genesis_balance.balance;
         }

         // Initialize vesting balances
         for( const auto& genesis_balance : config.sharedrop_balances.vesting_balances )
         {
            withdraw_vesting vesting;
            vesting.owner = convert_raw_address( genesis_balance.raw_address );
            vesting.start_time = config.sharedrop_balances.start_time;
            vesting.duration = fc::days( config.sharedrop_balances.duration_days ).to_seconds();
            vesting.original_balance = genesis_balance.balance;

            withdraw_condition condition( vesting, 0, 0 );
            balance_record initial_balance( condition );
            initial_balance.balance = vesting.original_balance;

            /* In case of redundant balances */
            const auto cur = self->get_balance_record( initial_balance.id() );
            if( cur.valid() ) initial_balance.balance += cur->balance;

            initial_balance.snapshot_info = snapshot_record( genesis_balance.raw_address, genesis_balance.balance );
            initial_balance.last_update = vesting.start_time;
            self->store_balance_record( initial_balance );

            total_base_supply += genesis_balance.balance;
         }

         // Initialize base asset
         asset_id_type asset_id = 0;
         asset_record base_asset;
         base_asset.id = asset_id;
         base_asset.symbol = BTS_BLOCKCHAIN_SYMBOL;
         base_asset.issuer_id = asset_record::god_issuer_id;
         base_asset.name = BTS_BLOCKCHAIN_NAME;
         base_asset.description = BTS_BLOCKCHAIN_DESCRIPTION;
         base_asset.precision = BTS_BLOCKCHAIN_PRECISION;
         base_asset.max_supply = BTS_BLOCKCHAIN_MAX_SHARES;
         base_asset.current_supply = total_base_supply;
         base_asset.authority_flag_permissions = 0;
         base_asset.registration_date = timestamp;
         base_asset.last_update = timestamp;
         self->store_asset_record( base_asset );

         // Initialize initial market assets
         for( const genesis_asset& asset : config.market_assets )
         {
             ++asset_id;
             asset_record rec;
             rec.id = asset_id;
             rec.symbol = asset.symbol;
             rec.issuer_id = asset_record::market_issuer_id;
             rec.name = asset.name;
             rec.description = asset.description;
             rec.precision = asset.precision;
             rec.max_supply = BTS_BLOCKCHAIN_MAX_SHARES;
             rec.authority_flag_permissions = 0;
             rec.registration_date = timestamp;
             rec.last_update = timestamp;
             self->store_asset_record( rec );
         }

         //add fork_data for the genesis block to the fork database
         block_fork_data gen_fork;
         gen_fork.is_valid = true;
         gen_fork.is_included = true;
         gen_fork.is_linked = true;
         gen_fork.is_known = true;
         _fork_db.store( block_id_type(), gen_fork );

         self->store_property_record( property_id_type::last_asset_id, variant( asset_id ) );
         self->store_property_record( property_id_type::last_account_id, variant( account_id ) );
         self->set_active_delegates( self->next_round_active_delegates() );
         self->set_statistics_enabled( statistics_enabled );

         return chain_id;
      } FC_CAPTURE_AND_RETHROW( (genesis_file)(statistics_enabled) ) }

      std::vector<block_id_type> chain_database_impl::fetch_blocks_at_number( uint32_t block_num )
      { try {
         const auto itr = _fork_number_db.find( block_num );
         if( itr.valid() ) return itr.value();
         return vector<block_id_type>();
      } FC_CAPTURE_AND_RETHROW( (block_num) ) }

      void chain_database_impl::clear_pending( const full_block& block_data )
      { try {
         for( const signed_transaction& trx : block_data.user_transactions )
            _pending_transaction_db.remove( trx.id() );

         _pending_fee_index.clear();
         _pending_trx_state = std::make_shared<pending_chain_state>( self->shared_from_this() );

         // this schedules the revalidate-pending-transactions task to execute in this thread
         // as soon as this current task (probably pushing a block) gets around to yielding.
         // This was changed from waiting on the old _revalidate_pending to prevent yielding
         // during the middle of pushing a block.  If that happens, the database is in an
         // inconsistent state and it confuses the p2p network code.
         // We skip this step if we are dealing with blocks prior to the last checkpointed block
         if( _head_block_header.block_num >= LAST_CHECKPOINT_BLOCK_NUM )
         {
             if( !_revalidate_pending.valid() || _revalidate_pending.ready() )
                 _revalidate_pending = fc::async( [=](){ revalidate_pending(); }, "revalidate_pending" );
         }
      } FC_CAPTURE_AND_RETHROW() }

      std::pair<block_id_type, block_fork_data> chain_database_impl::recursive_mark_as_linked(const std::unordered_set<block_id_type>& ids)
      {
         block_fork_data longest_fork;
         uint32_t highest_block_num = 0;
         block_id_type last_block_id;

         std::unordered_set<block_id_type> next_ids = ids;
         //while there are any next blocks for current block number being processed
         while( next_ids.size() )
         {
            std::unordered_set<block_id_type> pending; //builds list of all next blocks for the current block number being processed
            //mark as linked all blocks at the current block number being processed
            for( const block_id_type& next_id : next_ids )
            {
                block_fork_data record = _fork_db.fetch( next_id );
                record.is_linked = true;
                pending.insert( record.next_blocks.begin(), record.next_blocks.end() );
                //ilog( "store: ${id} => ${data}", ("id",next_id)("data",record) );
                _fork_db.store( next_id, record );

                //keep one of the block ids of the current block number being processed (simplify this code)
                const full_block& next_block = _block_id_to_full_block.fetch( next_id );
                if( next_block.block_num > highest_block_num )
                {
                    highest_block_num = next_block.block_num;
                    last_block_id = next_id;
                    longest_fork = record;
                }
            }
            next_ids = pending; //conceptually this increments the current block number being processed
         }

         return std::make_pair(last_block_id, longest_fork);
      }
      void chain_database_impl::recursive_mark_as_invalid(const std::unordered_set<block_id_type>& ids , const fc::exception& reason)
      {
         std::unordered_set<block_id_type> next_ids = ids;
         while( next_ids.size() )
         {
            std::unordered_set<block_id_type> pending;
            for( const block_id_type& next_id : next_ids )
            {
                block_fork_data record = _fork_db.fetch( next_id );
                assert(!record.valid()); //make sure we don't invalidate a previously validated record
                record.is_valid = false;
                record.invalid_reason = reason;
                pending.insert( record.next_blocks.begin(), record.next_blocks.end() );
                //ilog( "store: ${id} => ${data}", ("id",next_id)("data",record) );
                _fork_db.store( next_id, record );
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
       *  3) It provides the missing link between the genesis block and an existing chain
       *      - all next blocks need to be updated to change state to 'linked'
       *
       *  Returns the pair of the block id and block_fork_data of the block with the highest block number
       *  in the fork which contains the new block, in all of the above cases where the new block is linked;
       *  otherwise, returns the block id and fork data of the new block
       */
      std::pair<block_id_type, block_fork_data> chain_database_impl::store_and_index( const block_id_type& block_id,
                                                                                      const full_block& block_data )
      { try {
          //we should never try to store a block we've already seen (verify not in any of our databases)
          assert(!_block_id_to_full_block.fetch_optional(block_id));
          #ifndef NDEBUG
          {
            //check block id is not in fork_data, or if it is, make sure it's just a placeholder for block we are waiting for
            optional<block_fork_data> fork_data = _fork_db.fetch_optional(block_id);
            assert(!fork_data || !fork_data->is_known);
            //check block not in parallel_blocks database
            vector<block_id_type> parallel_blocks = fetch_blocks_at_number( block_data.block_num );
            assert( std::find(parallel_blocks.begin(), parallel_blocks.end(), block_id) == parallel_blocks.end());
          }
          #endif

          // first of all store this block at the given block number
          _block_id_to_full_block.store( block_id, block_data );

          if( self->get_statistics_enabled() )
          {
              block_record record;
              digest_block& temp = record;
              temp = digest_block( block_data );
              record.id = block_id;
              record.block_size = block_data.block_size();
              record.latency = blockchain::now() - block_data.timestamp;
              _block_id_to_block_record_db.store( block_id, record );
          }

          // update the parallel block list (fork_number_db):
          // get vector of all blocks with same block number, add this block to that list, then update the database
          vector<block_id_type> parallel_blocks = fetch_blocks_at_number( block_data.block_num );
          parallel_blocks.push_back( block_id );
          _fork_number_db.store( block_data.block_num, parallel_blocks );

          // Tell our previous block that we are one of it's next blocks (update previous block's next_blocks set)
          block_fork_data prev_fork_data;
          auto prev_itr = _fork_db.find( block_data.previous );
          if( prev_itr.valid() ) // we already know about its previous (note: we always know about genesis block)
          {
             ilog( "           we already know about its previous: ${p}", ("p",block_data.previous) );
             prev_fork_data = prev_itr.value();
          }
          else //if we don't know about the previous block even as a placeholder, create a placeholder for the previous block (placeholder block defaults as unlinked)
          {
             ilog( "           we don't know about its previous: ${p}", ("p",block_data.previous) );
             prev_fork_data.is_linked = false; //this is only a placeholder, we don't know what its previous block is, so it can't be linked
          }
          prev_fork_data.next_blocks.insert( block_id );
          _fork_db.store( block_data.previous, prev_fork_data );

          auto cur_itr = _fork_db.find( block_id );
          if( cur_itr.valid() ) //if placeholder was previously created for block
          {
            block_fork_data current_fork = cur_itr.value();
            current_fork.is_known = true; //was placeholder, now a known block
            ilog( "          current_fork: ${fork}", ("fork",current_fork) );
            ilog( "          prev_fork: ${prev_fork}", ("prev_fork",prev_fork_data) );
            // if new block is linked to genesis block, recursively mark all its next blocks as linked and return longest descendant block
            assert(!current_fork.is_linked);
            if( prev_fork_data.is_linked )
            {
              current_fork.is_linked = true;
              //if previous block is invalid, mark the new block as invalid too (block can't be valid if any previous block in its chain is invalid)
              bool prev_block_is_invalid = prev_fork_data.is_valid && !*prev_fork_data.is_valid;
              if (prev_block_is_invalid)
              {
                current_fork.is_valid = false;
                current_fork.invalid_reason = prev_fork_data.invalid_reason;
              }
              _fork_db.store( block_id, current_fork ); //update placeholder fork_block record with block data

              if (prev_block_is_invalid) //if previous block was invalid, mark all descendants as invalid and return current_block
              {
                recursive_mark_as_invalid(current_fork.next_blocks, *prev_fork_data.invalid_reason );
                return std::make_pair( block_id, current_fork );
              }
              else //we have a potentially viable alternate chain, mark the descendant blocks as linked and return the longest end block from descendant chains
              {
                std::pair<block_id_type,block_fork_data> longest_fork = recursive_mark_as_linked(current_fork.next_blocks);
                return longest_fork;
              }
            }
            else //this new block is not linked to genesis block, so no point in determining its longest descendant block, just return it and let it be skipped over
            {
              _fork_db.store( block_id, current_fork ); //update placeholder fork_block record with block data
              return std::make_pair( block_id, current_fork );
            }
          }
          else //no placeholder exists for this new block, just set its link flag
          {
            block_fork_data current_fork;
            current_fork.is_known = true;
            current_fork.is_linked = prev_fork_data.is_linked; //is linked if it's previous block is linked
            bool prev_block_is_invalid = prev_fork_data.is_valid && !*prev_fork_data.is_valid;
            if (prev_block_is_invalid)
            {
              current_fork.is_valid = false;
              current_fork.invalid_reason = prev_fork_data.invalid_reason;
            }
            //ilog( "          current_fork: ${id} = ${fork}", ("id",block_id)("fork",current_fork) );
            _fork_db.store( block_id, current_fork ); //add new fork_block record to database
            //this is first time we've seen this block mentioned, so we don't know about any linked descendants from it,
            //and therefore this is the last block in this chain that we know about, so just return that
            return std::make_pair( block_id, current_fork );
          }
      } FC_CAPTURE_AND_RETHROW( (block_id) ) }

      void chain_database_impl::mark_invalid(const block_id_type& block_id , const fc::exception& reason)
      {
         // fetch the fork data for block_id, mark it as invalid and
         // then mark every item after it as invalid as well.
         auto fork_data = _fork_db.fetch( block_id );
         assert(!fork_data.valid()); //make sure we're not invalidating a block that we previously have validated
         fork_data.is_valid = false;
         fork_data.invalid_reason = reason;
         _fork_db.store( block_id, fork_data );
         recursive_mark_as_invalid( fork_data.next_blocks, reason );
      }

      void chain_database_impl::mark_as_unchecked(const block_id_type& block_id)
      {
        // fetch the fork data for block_id, mark it as unchecked
        auto fork_data = _fork_db.fetch( block_id );
        assert(!fork_data.valid()); //make sure we're not unchecking a block that we previously have validated
        fork_data.is_valid.reset(); //mark as unchecked (i.e. we will check validity again later during switch_to_fork)
        fork_data.invalid_reason.reset();
        dlog( "store: ${id} => ${data}", ("id",block_id)("data",fork_data) );
        _fork_db.store( block_id, fork_data );
        // then mark every block after it as unchecked as well.
        std::unordered_set<block_id_type>& next_ids = fork_data.next_blocks;
        while( next_ids.size() )
        {
          std::unordered_set<block_id_type> pending_blocks_for_next_loop_iteration;
          for( const auto& next_block_id : next_ids )
          {
            block_fork_data record = _fork_db.fetch( next_block_id );
            record.is_valid.reset(); //mark as unchecked (i.e. we will check validity again later during switch_to_fork)
            record.invalid_reason.reset();
            pending_blocks_for_next_loop_iteration.insert( record.next_blocks.begin(), record.next_blocks.end() );
            dlog( "store: ${id} => ${data}", ("id",next_block_id)("data",record) );
            _fork_db.store( next_block_id, record );
          }
          next_ids = pending_blocks_for_next_loop_iteration;
        }
      }

      void chain_database_impl::mark_included( const block_id_type& block_id, bool included )
      { try {
         //ilog( "included: ${block_id} = ${state}", ("block_id",block_id)("state",included) );
         auto fork_data = _fork_db.fetch( block_id );
         //if( fork_data.is_included != included )
         {
            fork_data.is_included = included;
            if( included )
            {
               fork_data.is_valid  = true;
            }
            //ilog( "store: ${id} => ${data}", ("id",block_id)("data",fork_data) );
            _fork_db.store( block_id, fork_data );
         }
         // fetch the fork data for block_id, mark it as included and
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id)("included",included) ) }

      void chain_database_impl::switch_to_fork( const block_id_type& block_id )
      { try {
         if (block_id == _head_block_id) //if block_id is current head block, do nothing
           return; //this is necessary to avoid unnecessarily popping the head block in this case

         ilog( "switch from fork ${id} to ${to_id}", ("id",_head_block_id)("to_id",block_id) );
         vector<block_id_type> history = get_fork_history( block_id );
         while( history.back() != _head_block_id )
         {
            ilog( "    pop ${id}", ("id",_head_block_id) );
            pop_block();
         }
         for( int32_t i = history.size()-2; i >= 0 ; --i )
         {
            ilog( "    extend ${i}", ("i",history[i]) );
            extend_chain( self->get_block( history[i] ) );
         }
      } FC_CAPTURE_AND_RETHROW( (block_id) ) }

      void chain_database_impl::apply_transactions( const full_block& block_data,
                                                    const pending_chain_state_ptr& pending_state )const
      { try {
         uint32_t trx_num = 0;
         for( const auto& trx : block_data.user_transactions )
         {
            transaction_evaluation_state_ptr trx_eval_state = std::make_shared<transaction_evaluation_state>( pending_state );
            trx_eval_state->_skip_signature_check = !self->_verify_transaction_signatures;
            trx_eval_state->evaluate( trx );

            const transaction_id_type& trx_id = trx.id();
            otransaction_record record = pending_state->lookup<transaction_record>( trx_id );
            FC_ASSERT( record.valid() );
            record->chain_location = transaction_location( block_data.block_num, trx_num );
            pending_state->store_transaction( trx_id, *record );

            ++trx_num;
         }
      } FC_CAPTURE_AND_RETHROW() }

      void chain_database_impl::pay_delegate( const block_id_type& block_id,
                                              const public_key_type& block_signee,
                                              const pending_chain_state_ptr& pending_state,
                                              oblock_record& record )const
      { try {
          if( pending_state->get_head_block_num() < BTS_V0_6_0_FORK_BLOCK_NUM )
              return pay_delegate_v2( block_id, block_signee, pending_state, record );

          oasset_record base_asset_record = pending_state->get_asset_record( asset_id_type( 0 ) );
          FC_ASSERT( base_asset_record.valid() );

          oaccount_record delegate_record = self->get_account_record( address( block_signee ) );
          FC_ASSERT( delegate_record.valid() );
          delegate_record = pending_state->get_account_record( delegate_record->id );
          FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() && delegate_record->delegate_info.valid() );

          const uint8_t pay_rate_percent = delegate_record->delegate_info->pay_rate;
          FC_ASSERT( pay_rate_percent >= 0 && pay_rate_percent <= 100 );

          const share_type max_new_shares = self->get_max_delegate_pay_issued_per_block();
          const share_type accepted_new_shares = (max_new_shares * pay_rate_percent) / 100;
          FC_ASSERT( max_new_shares >= 0 && accepted_new_shares >= 0 );
          base_asset_record->current_supply += accepted_new_shares;

          static const uint32_t blocks_per_two_weeks = 14 * BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
          const share_type max_collected_fees = base_asset_record->collected_fees / blocks_per_two_weeks;
          const share_type accepted_collected_fees = (max_collected_fees * pay_rate_percent) / 100;
          const share_type destroyed_collected_fees = max_collected_fees - accepted_collected_fees;
          FC_ASSERT( max_collected_fees >= 0 && accepted_collected_fees >= 0 && destroyed_collected_fees >= 0 );
          base_asset_record->collected_fees -= max_collected_fees;
          base_asset_record->current_supply -= destroyed_collected_fees;

          const share_type accepted_paycheck = accepted_new_shares + accepted_collected_fees;
          FC_ASSERT( accepted_paycheck >= 0 );
          delegate_record->delegate_info->votes_for += accepted_paycheck;
          delegate_record->delegate_info->pay_balance += accepted_paycheck;
          delegate_record->delegate_info->total_paid += accepted_paycheck;

          pending_state->store_account_record( *delegate_record );
          pending_state->store_asset_record( *base_asset_record );

          if( record.valid() )
          {
              record->signee_shares_issued = accepted_new_shares;
              record->signee_fees_collected = accepted_collected_fees;
              record->signee_fees_destroyed = destroyed_collected_fees;
          }
      } FC_CAPTURE_AND_RETHROW( (block_id)(block_signee)(record) ) }

      void chain_database_impl::save_undo_state( const uint32_t block_num,
                                                 const block_id_type& block_id,
                                                 const pending_chain_state_ptr& pending_state )
      { try {
          if( block_num < _min_undo_block )
              return;

          pending_chain_state_ptr undo_state = std::make_shared<pending_chain_state>( pending_state );
          pending_state->build_undo_state( undo_state );

          if( block_num > BTS_BLOCKCHAIN_MAX_UNDO_HISTORY )
          {
              const uint32_t old_block_num = block_num - BTS_BLOCKCHAIN_MAX_UNDO_HISTORY;
              const block_id_type& old_block_id = self->get_block_id( old_block_num );
              _block_id_to_undo_state.remove( old_block_id );
          }

          _block_id_to_undo_state.store( block_id, *undo_state );
      } FC_CAPTURE_AND_RETHROW( (block_num)(block_id) ) }

      void chain_database_impl::verify_header( const digest_block& block_digest, const public_key_type& block_signee )const
      { try {
          if( block_digest.block_num > 1 && block_digest.block_num != _head_block_header.block_num + 1 )
             FC_CAPTURE_AND_THROW( block_numbers_not_sequential, (block_digest)(_head_block_header) );
          if( block_digest.previous  != _head_block_id )
             FC_CAPTURE_AND_THROW( invalid_previous_block_id, (block_digest)(_head_block_id) );
          if( block_digest.timestamp.sec_since_epoch() % BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC != 0 )
             FC_CAPTURE_AND_THROW( invalid_block_time );
          if( block_digest.block_num > 1 && block_digest.timestamp <= _head_block_header.timestamp )
             FC_CAPTURE_AND_THROW( time_in_past, (block_digest.timestamp)(_head_block_header.timestamp) );

          fc::time_point_sec now = bts::blockchain::now();
          const uint32_t delta_seconds = (block_digest.timestamp - now).to_seconds();
          if( block_digest.timestamp >  (now + BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC*2) )
             FC_CAPTURE_AND_THROW( time_in_future, (block_digest.timestamp)(now)(delta_seconds) );

          if( block_digest.block_num >= BTS_V0_9_0_FORK_BLOCK_NUM )
          {
              if( NOT block_digest.validate_digest() )
                 FC_CAPTURE_AND_THROW( invalid_block_digest );
          }

          FC_ASSERT( block_digest.validate_unique() );

          const auto expected_delegate = self->get_slot_signee( block_digest.timestamp, self->get_active_delegates() );

          if( block_signee != expected_delegate.signing_key() )
             FC_CAPTURE_AND_THROW( invalid_delegate_signee, (expected_delegate.id) );
      } FC_CAPTURE_AND_RETHROW( (block_digest)(block_signee) ) }

      void chain_database_impl::update_head_block( const signed_block_header& block_header,
                                                   const block_id_type& block_id )
      { try {
          _head_block_header = block_header;
          _head_block_id = block_id;
      } FC_CAPTURE_AND_RETHROW( (block_header)(block_id) ) }

      /**
       *  A block should be produced every BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC. If we do not have a
       *  block for any multiple of this interval between produced_block and the current head block
       *  then we need to lookup the delegates that should have produced a block during that interval
       *  and increment their blocks_missed.
       *
       *  We also need to increment the blocks_produced for the delegate that actually produced the
       *  block.
       *
       *  Note that produced_block has already been verified by the caller and that updates are
       *  applied to pending_state.
       */
      void chain_database_impl::update_delegate_production_info( const block_header& block_header,
                                                                 const block_id_type& block_id,
                                                                 const public_key_type& block_signee,
                                                                 const pending_chain_state_ptr& pending_state )const
      { try {
          /* Update production info for signing delegate */
          account_id_type delegate_id = self->get_delegate_record_for_signee( block_signee ).id;
          oaccount_record delegate_record = pending_state->get_account_record( delegate_id );
          FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
          delegate_stats& delegate_info = *delegate_record->delegate_info;

          /* Validate secret */
          if( delegate_info.next_secret_hash.valid() )
          {
              const secret_hash_type hash_of_previous_secret = fc::ripemd160::hash( block_header.previous_secret );
              FC_ASSERT( hash_of_previous_secret == *delegate_info.next_secret_hash,
                         "",
                         ("previous_secret",block_header.previous_secret)
                         ("hash_of_previous_secret",hash_of_previous_secret)
                         ("delegate_record",delegate_record) );
          }

          delegate_info.blocks_produced += 1;
          delegate_info.next_secret_hash = block_header.next_secret_hash;
          delegate_info.last_block_num_produced = block_header.block_num;
          pending_state->store_account_record( *delegate_record );

          if( self->get_statistics_enabled() )
          {
              const slot_record slot( block_header.timestamp, delegate_id, block_id );
              pending_state->store_slot_record( slot );
          }

          /* Update production info for missing delegates */

          time_point_sec block_timestamp;
          auto head_block = self->get_head_block();
          if( head_block.block_num > 0 ) block_timestamp = head_block.timestamp + BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
          else block_timestamp = block_header.timestamp;
          const auto& active_delegates = self->get_active_delegates();

          for( ; block_timestamp < block_header.timestamp; block_timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC )
          {
              /* Note: Active delegate list has not been updated yet so we can safely use the timestamp */
              delegate_id = self->get_slot_signee( block_timestamp, active_delegates ).id;
              delegate_record = pending_state->get_account_record( delegate_id );
              FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );

              delegate_record->delegate_info->blocks_missed += 1;
              pending_state->store_account_record( *delegate_record );

              if( self->get_statistics_enabled() )
                  pending_state->store_slot_record( slot_record( block_timestamp, delegate_id )  );
          }
      } FC_CAPTURE_AND_RETHROW( (block_header)(block_id)(block_signee) ) }

      void chain_database_impl::update_random_seed( const secret_hash_type& new_secret,
                                                    const pending_chain_state_ptr& pending_state,
                                                    oblock_record& record )const
      { try {
         const auto current_seed = pending_state->get_current_random_seed();
         fc::sha512::encoder enc;
         fc::raw::pack( enc, new_secret );
         fc::raw::pack( enc, current_seed );
         const auto& new_seed = fc::ripemd160::hash( enc.result() );
         pending_state->store_property_record( property_id_type::last_random_seed_id, variant( new_seed ) );
         if( record.valid() ) record->random_seed = new_seed;
      } FC_CAPTURE_AND_RETHROW( (new_secret)(record) ) }

      void chain_database_impl::update_active_delegate_list( const uint32_t block_num,
                                                             const pending_chain_state_ptr& pending_state )const
      { try {
          if( pending_state->get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
              return update_active_delegate_list_v1( block_num, pending_state );

          if( block_num % BTS_BLOCKCHAIN_NUM_DELEGATES != 0 )
              return;

          auto active_del = self->next_round_active_delegates();
          const size_t num_del = active_del.size();

          // Perform a random shuffle of the sorted delegate list.
          fc::sha256 rand_seed = fc::sha256::hash( pending_state->get_current_random_seed() );
          for( uint32_t i=0, x=0; i < num_del; i++ )
          {
             // we only use xth element of hash once,
             // then when all 4 elements have been used,
             // we re-mix the hash by running sha256() again
             //
             // the algorithm used is the second algorithm described in
             // http://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#The_modern_algorithm
             //
             // previous implementation suffered from bias due to
             // picking from all elements, see
             // http://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#Implementation_errors
             // in addition to various problems related to
             // pre-increment operation
             //
             uint64_t r = rand_seed._hash[x];
             uint32_t choices = num_del - i;
             uint32_t j = ((uint32_t) (r % choices)) + i;

             std::swap( active_del[ i ], active_del[ j ] );

             x = (x + 1) & 3;
             if( x == 0 )
                 rand_seed = fc::sha256::hash( rand_seed );
          }

          pending_state->set_active_delegates( active_del );
      } FC_CAPTURE_AND_RETHROW( (block_num) ) }

      void chain_database_impl::execute_markets( const time_point_sec timestamp,
                                                 const pending_chain_state_ptr& pending_state )const
      { try {
          if( pending_state->get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
              return execute_markets_v1( timestamp, pending_state );

          vector<market_transaction> market_transactions;

          const auto dirty_markets = self->get_dirty_markets();
          for( const auto& market_pair : dirty_markets )
          {
              FC_ASSERT( market_pair.first > market_pair.second );
              market_engine engine( pending_state, *this );
              if( engine.execute( market_pair.first, market_pair.second, timestamp ) )
              {
                  market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(),
                                                                         engine._market_transactions.end() );
              }
          }

          pending_state->set_market_transactions( std::move( market_transactions ) );

          if( self->_debug_verify_market_matching )
              debug_check_no_orders_overlap( pending_state );
      } FC_CAPTURE_AND_RETHROW( (timestamp) ) }

      void chain_database_impl::debug_check_no_orders_overlap(
          const pending_chain_state_ptr& pending_state ) const
      {
          //
          // the market engine does all kinds of fancy indexing to find
          // and fill overlapping orders efficiently
          //
          // this method only exists for debugging purposes, so we
          // don't care about efficiency.  we simply dump the orders
          // and check the top bid-side order vs. top ask-side order
          // for each market
          //
          // since this method doesn't fill orders, there is no fill
          // logic here
          //

          ilog("running debug_check_no_orders_overlap()");

          std::map<std::pair<asset_id_type, asset_id_type>, std::pair<price, market_order>> mkt_to_best_bid;
          std::map<std::pair<asset_id_type, asset_id_type>, std::pair<price, market_order>> mkt_to_best_ask;

          auto process_bid = [&]( price p, market_order& order )
          {
              std::pair<asset_id_type, asset_id_type> mkt = p.asset_pair();
              auto current_best = mkt_to_best_bid.find( mkt );
              if( (current_best == mkt_to_best_bid.end()) ||
                  (p > current_best->second.first) )
                  mkt_to_best_bid[mkt] = std::pair<price, market_order>( p, order );
          };

          auto process_ask = [&]( price p, market_order& order )
          {
              std::pair<asset_id_type, asset_id_type> mkt = p.asset_pair();
              auto current_best = mkt_to_best_ask.find( mkt );
              if( (current_best == mkt_to_best_ask.end()) ||
                  (p < current_best->second.first) )
                  mkt_to_best_ask[mkt] = std::pair<price, market_order>( p, order );
          };

          for( bts::db::cached_level_map< market_index_key, order_record >::iterator
               bid_itr  = _bid_db.begin();
               bid_itr.valid();
               bid_itr++ )
          {
              oorder_record orec = pending_state->get_bid_record( bid_itr.key() );
              if( (!orec.valid()) || (orec->balance <= 0) )
                  continue;

              market_order morder = market_order( bid_order, bid_itr.key(), *orec );
              process_bid( morder.get_price(), morder );
          }

          for( bts::db::cached_level_map< market_index_key, order_record >::iterator
               ask_itr  = _ask_db.begin();
               ask_itr.valid();
               ask_itr++ )
          {
              oorder_record orec = pending_state->get_ask_record( ask_itr.key() );
              if( (!orec.valid()) || (orec->balance <= 0) )
                  continue;

              market_order morder = market_order( ask_order, ask_itr.key(), *orec );
              process_ask( morder.get_price(), morder );
          }

          for( bts::db::cached_level_map< market_index_key, order_record >::iterator
               short_itr  = _short_db.begin();
               short_itr.valid();
               short_itr++ )
          {
              oorder_record orec = pending_state->get_short_record( short_itr.key() );
              if( (!orec.valid()) || (orec->balance <= 0) )
                  continue;

              market_order morder = market_order( short_order, short_itr.key(), *orec );
              oprice feed = self->get_active_feed_price( morder.get_price().quote_asset_id );
              if( !feed.valid() )
                  continue;
              // TODO:  use different ctor
              process_bid( morder.get_price( *feed ), morder );
          }

          for( bts::db::cached_level_map< market_index_key, collateral_record >::iterator
               collateral_itr  = _collateral_db.begin();
               collateral_itr.valid();
               collateral_itr++ )
          {
              market_index_key k = collateral_itr.key();
              oprice feed = self->get_active_feed_price( k.order_price.quote_asset_id );
              if( !feed.valid() )
                  continue;

              ocollateral_record orec = pending_state->get_collateral_record( k );
              if( (!orec.valid()) || (orec->payoff_balance <= 0) || (orec->collateral_balance <= 0) )
                  continue;

              market_order morder = market_order( cover_order,
                k,
                order_record(orec->payoff_balance),
                orec->collateral_balance,
                orec->interest_rate,
                orec->expiration
                );

              if( k.order_price >= (*feed) )
                  // margin called.  in reality the price may be raised
                  // to force a match, but we'll ignore that subtlety
                  // here...
                  process_ask( (*feed), morder );
              else if( orec->expiration <= self->now() )
                  // expired
                  process_ask( (*feed), morder );
          }

          uint64_t failure_count = 0;
          fc::variants error_list;

          // best bid price <= best ask price
          for( auto it=mkt_to_best_bid.begin(); it != mkt_to_best_bid.end(); it++ )
          {
              std::pair<asset_id_type, asset_id_type> k = it->first;
              auto it_ask = mkt_to_best_ask.find( k );
              if( it_ask == mkt_to_best_ask.end() )
                  continue;
              price bid_price = it->second.first;
              price ask_price = it_ask->second.first;
              if( bid_price < ask_price )
                  continue;
              oasset_record quote_asset = self->get_asset_record( k.first );
              oasset_record base_asset = self->get_asset_record( k.second );

              // it and it_ask are of type
              // ((asset_id_type, asset_id_type), (price, market_order))
              market_order bid_order = it->second.second;
              market_order ask_order = it_ask->second.second;

              std::string bid_type, ask_type;

              // there is probably a way to do this with fc reflection
              // but the lack of documentation means I don't know what
              // it is or how to easily find it out

              auto get_order_type = [&]( const market_order& order ) -> std::string
              {
                  switch( order.type )
                  {
                      case bts::blockchain::null_order:
                          return "null_order";
                      case bts::blockchain::bid_order:
                          return "bid_order";
                      case bts::blockchain::ask_order:
                          return "ask_order";
                      case bts::blockchain::short_order:
                          return "short_order";
                      case bts::blockchain::cover_order:
                          return "cover_order";
                      default:
                          return "<unknown>";
                  }
              };

              error_list.push_back(
                   fc::mutable_variant_object()
                   ("block", self->get_head_block_num())
                   ("quote_asset", quote_asset->symbol)
                   ("base_asset", base_asset->symbol)
                   ("bid_price", it->second.first)
                   ("ask_price", it_ask->second.first)
                   ("bid_type", get_order_type( bid_order ))
                   ("ask_type", get_order_type( ask_order ))
                  );

              FC_ASSERT( quote_asset.valid() );
              FC_ASSERT( base_asset.valid() );

              elog("mismatch in market ${quote} / ${base} at block ${block}",
                  ("quote", quote_asset->symbol)
                  ("base" ,  base_asset->symbol)
                  ("block", self->get_head_block_num())
                  );
              failure_count++;
          }
          if( failure_count == 0 )
          {
              ilog("debug_check_no_orders_overlap() on block ${b} ok",
                   ("b", self->get_head_block_num())
                  );
          }
          else
          {
              elog("debug_check_no_orders_overlap() on block ${b} found ${n} error(s)",
                   ("b", self->get_head_block_num())
                   ("n", failure_count)
                  );
              _debug_matching_error_log.push_back( error_list );
          }

          return;
      }

      /**
       *  Performs all of the block validation steps and throws if error.
       */
      void chain_database_impl::extend_chain( const full_block& block_data )
      { try {
         const time_point start_time = time_point::now();
         const block_id_type& block_id = block_data.id();
         if( _debug_trap_blocks.count( block_data.block_num ) != 0 )
         {
#ifdef WIN32
             __debugbreak();
#else
             raise(SIGTRAP);
#endif
         }
         try
         {
            public_key_type block_signee;
            if( block_data.block_num > LAST_CHECKPOINT_BLOCK_NUM )
            {
                block_signee = block_data.signee();
            }
            else
            {
                const auto iter = CHECKPOINT_BLOCKS.find( block_data.block_num );
                if( iter != CHECKPOINT_BLOCKS.end() && iter->second != block_id )
                    FC_CAPTURE_AND_THROW( failed_checkpoint_verification, (block_id)(*iter) );

                // Skip signature validation
                block_signee = self->get_slot_signee( block_data.timestamp, self->get_active_delegates() ).signing_key();
            }

            // NOTE: Secret is validated later in update_delegate_production_info()
            verify_header( digest_block( block_data ), block_signee );

            // Create a pending state to track changes that would apply as we evaluate the block
            pending_chain_state_ptr pending_state = std::make_shared<pending_chain_state>( self->shared_from_this() );

            /** Increment the blocks produced or missed for all delegates. This must be done
             *  before applying transactions because it depends upon the current active delegate order.
             **/
            update_delegate_production_info( block_data, block_id, block_signee, pending_state );

            oblock_record block_record;
            if( self->get_statistics_enabled() ) block_record = self->get_block_record( block_id );

            pay_delegate( block_id, block_signee, pending_state, block_record );

            if( block_data.block_num < BTS_V0_4_9_FORK_BLOCK_NUM )
                apply_transactions( block_data, pending_state );

            execute_markets( block_data.timestamp, pending_state );

            if( block_data.block_num >= BTS_V0_4_9_FORK_BLOCK_NUM )
                apply_transactions( block_data, pending_state );

            update_active_delegate_list( block_data.block_num, pending_state );

            update_random_seed( block_data.previous_secret, pending_state, block_record );

#ifdef BTS_TEST_NETWORK
            pending_state->check_supplies();
#endif

            save_undo_state( block_data.block_num, block_id, pending_state );

            pending_state->apply_changes();

            update_head_block( block_data, block_id );

            mark_included( block_id, true );

            clear_pending( block_data );

            _block_num_to_id_db.store( block_data.block_num, block_id );

            // NOTE: None of the following hardfork changes can be rewound

            if( block_data.block_num == BTS_V0_4_16_FORK_BLOCK_NUM )
            {
                wlog( "Block ${n} Hardfork: Recalculating supply for base asset", ("n",block_data.block_num) );
                oasset_record base_asset_record = self->get_asset_record( asset_id_type( 0 ) );
                FC_ASSERT( base_asset_record.valid() );
                base_asset_record->current_supply = self->calculate_supplies().at( 0 );
                self->store_asset_record( *base_asset_record );
            }
            else if( block_data.block_num == BTS_V0_4_17_FORK_BLOCK_NUM
                     || block_data.block_num == BTS_V0_4_21_FORK_BLOCK_NUM
                     || block_data.block_num == BTS_V0_4_24_FORK_BLOCK_NUM
                     || block_data.block_num == BTS_V0_9_0_FORK_BLOCK_NUM )
            {
                vector<asset_record> records;
                for( auto iter = _asset_id_to_record.unordered_begin(); iter != _asset_id_to_record.unordered_end(); ++iter )
                    records.push_back( iter->second );

                wlog( "Block ${n} Hardfork: Recalculating supply for ${x} assets", ("n",block_data.block_num)("x",records.size()) );

                auto supplies = self->calculate_supplies();
                auto debts = self->calculate_debts( false );

                for( auto& record : records )
                {
                    share_type supply = supplies[ record.id ];
                    share_type fees = record.collected_fees;

                    if( record.is_market_issued() )
                    {
                        share_type debt = debts[ record.id ];
                        if( supply != debt )
                        {
                            const share_type difference = debt - supply;
                            supply += difference;
                            fees += difference;
                        }
                    }

                    record.current_supply = supply;
                    record.collected_fees = fees;
                    self->store_asset_record( record );
                }
            }

            if( block_data.block_num == BTS_V0_4_24_FORK_BLOCK_NUM )
            {
                vector<account_record> records;
                for( auto iter = _account_id_to_record.unordered_begin(); iter != _account_id_to_record.unordered_end(); ++iter )
                {
                    const auto& record = iter->second;
                    if( record.is_delegate() )
                        records.push_back( record );
                }

                wlog( "Block ${n} Hardfork: Resetting pay rates for ${x} delegates", ("n",block_data.block_num)("x",records.size()) );

                for( auto& record : records )
                {
                    record.delegate_info->pay_rate = 3;
                    self->store_account_record( record );
                }
            }

            if( block_data.block_num == BTS_V0_9_0_FORK_BLOCK_NUM )
            {
                static const fc::uint128 max_apr = fc::uint128( BTS_BLOCKCHAIN_MAX_SHORT_APR_PCT ) * FC_REAL128_PRECISION / 100;

                map<market_index_key, collateral_record> records;
                for( auto iter = _collateral_db.begin(); iter.valid(); ++iter )
                {
                    const collateral_record& record = iter.value();
                    if( record.interest_rate.ratio > max_apr )
                        records[ iter.key() ] = record;
                }

                wlog( "Block ${n} Hardfork: Bounding interest rates for ${x} cover positions", ("n",block_data.block_num)("x",records.size()) );

                for( auto& item : records )
                {
                    const market_index_key& key = item.first;
                    collateral_record& record = item.second;
                    record.interest_rate.ratio = std::min( max_apr, record.interest_rate.ratio );
                    self->store_collateral_record( key, record );
                }
            }

            if( block_record.valid() )
            {
                block_record->processing_time = time_point::now() - start_time;
                _block_id_to_block_record_db.store( block_id, *block_record );
            }
         }
         catch ( const fc::exception& e )
         {
            wlog( "error applying block: ${e}", ("e",e.to_detail_string() ));
            mark_invalid( block_id, e );
            throw;
         }

         // Purge expired transactions from unique cache
         auto iter = _unique_transactions.begin();
         while( iter != _unique_transactions.end() && iter->expiration <= self->now() )
             iter = _unique_transactions.erase( iter );

         // Schedule the observer notifications for later; the chain is in a
         // non-premptable state right now, and observers may yield
         if( (blockchain::now() - block_data.timestamp).to_seconds() < BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC )
         {
             for( chain_observer* o : _observers )
             {
                 fc::async( [ = ] { o->block_pushed( block_data ); }, "call_block_pushed_observer" );
             }
         }
      } FC_CAPTURE_AND_RETHROW() }

      /**
       * Traverse the previous links of all blocks in fork until we find one that is_included
       *
       * The last item in the result will be the only block id that is already included in
       * the blockchain.
       */
      std::vector<block_id_type> chain_database_impl::get_fork_history( const block_id_type& id )
      { try {
         std::vector<block_id_type> history;
         history.push_back( id );

         block_id_type next_id = id;
         while( true )
         {
            auto header = self->get_block_header( next_id );
            //ilog( "header: ${h}", ("h",header) );
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
      } FC_CAPTURE_AND_RETHROW( (id) ) }

      void chain_database_impl::pop_block()
      { try {
         assert(_head_block_header.block_num != 0);
         if( _head_block_header.block_num == 0 )
         {
            elog( "attempting to pop block 0" );
            return;
         }

         // update the is_included flag on the fork data
         mark_included( _head_block_id, false );

         // update the block_num_to_block_id index
         _block_num_to_id_db.remove( _head_block_header.block_num );

         auto previous_block_id = _head_block_header.previous;

         const auto undo_iter = _block_id_to_undo_state.unordered_find( _head_block_id );
         FC_ASSERT( undo_iter != _block_id_to_undo_state.unordered_end() );
         const auto& undo_state = undo_iter->second;

         bts::blockchain::pending_chain_state_ptr undo_state_ptr = std::make_shared<bts::blockchain::pending_chain_state>( undo_state );
         undo_state_ptr->set_prev_state( self->shared_from_this() );
         undo_state_ptr->apply_changes();

         _head_block_id = previous_block_id;

         if( _head_block_id == block_id_type() )
             _head_block_header = signed_block_header();
         else
             _head_block_header = self->get_block_header( _head_block_id );

         // Schedule the observer notifications for later; the chain is in a
         // non-premptable state right now, and observers may yield
         for( chain_observer* o : _observers )
         {
             fc::async( [ = ] { o->block_popped( undo_state_ptr ); }, "call_block_popped_observer" );
         }
      } FC_CAPTURE_AND_RETHROW() }

   } // namespace detail

   chain_database::chain_database()
   :my( new detail::chain_database_impl() )
   {
      my->self = this;
   }

   chain_database::~chain_database()
   {
      try
      {
         close();
      }
      catch( const fc::exception& e )
      {
         elog( "unexpected exception closing database\n ${e}", ("e",e.to_detail_string() ) );
      }
   }

   std::vector<account_id_type> chain_database::next_round_active_delegates()const
   {
       return get_delegates_by_vote( 0, BTS_BLOCKCHAIN_NUM_DELEGATES );
   }

   std::vector<account_id_type> chain_database::get_delegates_by_vote( uint32_t first, uint32_t count )const
   { try {
      auto del_vote_itr = my->_delegate_votes.begin();
      std::vector<account_id_type> sorted_delegates;
      sorted_delegates.reserve( count );
      uint32_t pos = 0;
      while( sorted_delegates.size() < count && del_vote_itr != my->_delegate_votes.end() )
      {
         if( pos >= first )
            sorted_delegates.push_back( del_vote_itr->delegate_id );
         ++pos;
         ++del_vote_itr;
      }
      return sorted_delegates;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void chain_database::open( const fc::path& data_dir, const fc::optional<fc::path>& genesis_file, const bool statistics_enabled,
                              const std::function<void(float)> replay_status_callback )
   { try {
      std::exception_ptr error_opening_database;
      try
      {
          //This function will yield the first time it is called. Do that now, before calling push_block
          now();

          my->load_checkpoints( data_dir.parent_path() );

          if( !my->replay_required( data_dir ) )
          {
              my->open_database( data_dir );

              uint32_t head_block_num = 0;
              block_id_type head_block_id;
              my->_block_num_to_id_db.last( head_block_num, head_block_id );

              if( head_block_num > 0 )
              {
                  my->_head_block_id = head_block_id;;
                  my->_head_block_header = get_block_header( head_block_id );
              }

              my->populate_indexes();
          }
          else
          {
              wlog( "Database inconsistency detected; erasing state and attempting to replay blockchain" );

              fc::remove_all( data_dir / "index" );

              if( fc::is_directory( data_dir / "raw_chain/block_id_to_block_data_db" ) )
              {
                  if( !fc::is_directory( data_dir / "raw_chain/block_id_to_data_original" ) )
                      fc::rename( data_dir / "raw_chain/block_id_to_block_data_db", data_dir / "raw_chain/block_id_to_data_original" );
              }

              // During replay we implement stop-and-copy garbage collection on the raw blocks
              decltype( my->_block_id_to_full_block ) block_id_to_data_original;
              block_id_to_data_original.open( data_dir / "raw_chain/block_id_to_data_original" );
              const size_t original_size = fc::directory_size( data_dir / "raw_chain/block_id_to_data_original" );

              my->open_database( data_dir );
              store_property_record( property_id_type::database_version, variant( BTS_BLOCKCHAIN_DATABASE_VERSION ) );

              const auto toggle_leveldb = [ this ]( const bool enabled )
              {
                  my->_block_id_to_undo_state.toggle_leveldb( enabled );

                  my->_property_id_to_record.toggle_leveldb( enabled );

                  my->_account_id_to_record.toggle_leveldb( enabled );
                  my->_account_name_to_id.toggle_leveldb( enabled );
                  my->_account_address_to_id.toggle_leveldb( enabled );

                  my->_asset_id_to_record.toggle_leveldb( enabled );
                  my->_asset_symbol_to_id.toggle_leveldb( enabled );

                  my->_slate_id_to_record.toggle_leveldb( enabled );

                  my->_balance_id_to_record.toggle_leveldb( enabled );
              };

              const auto set_db_cache_write_through = [ this ]( bool write_through )
              {
                  my->_burn_index_to_record.set_write_through( write_through );

                  my->_status_index_to_record.set_write_through( write_through );

                  my->_feed_index_to_record.set_write_through( write_through );

                  my->_ask_db.set_write_through( write_through );
                  my->_bid_db.set_write_through( write_through );
                  my->_short_db.set_write_through( write_through );
                  my->_collateral_db.set_write_through( write_through );

                  my->_market_transactions_db.set_write_through( write_through );
                  my->_market_history_db.set_write_through( write_through );
              };

              // For the duration of replaying, we allow certain databases to postpone flushing until we finish
              toggle_leveldb( false );
              set_db_cache_write_through( false );

              my->initialize_genesis( genesis_file, statistics_enabled );

              // Load block num -> id db into memory and clear from disk for replaying
              map<uint32_t, block_id_type> num_to_id;
              {
                  for( auto itr = my->_block_num_to_id_db.begin(); itr.valid(); ++itr )
                      num_to_id.emplace_hint( num_to_id.end(), itr.key(), itr.value() );

                  my->_block_num_to_id_db.close();
                  fc::remove_all( data_dir / "raw_chain/block_num_to_id_db" );
                  my->_block_num_to_id_db.open( data_dir / "raw_chain/block_num_to_id_db" );
              }

              if( !replay_status_callback )
              {
                  std::cout << "Please be patient, this will take several minutes...\r\nReplaying blockchain..."
                            << std::flush << std::fixed;
              }
              else
              {
                  replay_status_callback( 0 );
              }

              uint32_t blocks_indexed = 0;
              const auto total_blocks = num_to_id.size();
              const auto genesis_time = get_genesis_timestamp();
              const auto start_time = blockchain::now();

              const auto insert_block = [&]( const full_block& block )
              {
                  if( blocks_indexed % 200 == 0 )
                  {
                      float progress;
                      if( total_blocks )
                      {
                          progress = float( blocks_indexed ) / total_blocks;
                      }
                      else
                      {
                          const auto seconds = (start_time - genesis_time).to_seconds();
                          progress = float( blocks_indexed * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC ) / seconds;
                      }

                      progress *= 100;

                      if( !replay_status_callback )
                      {
                          std::cout << "\rReplaying blockchain... "
                                       "Approximately " << std::setprecision( 2 ) << progress << "% complete." << std::flush;
                      }
                      else
                      {
                          replay_status_callback( progress );
                      }
                  }

                  push_block( block );
                  ++blocks_indexed;

                  if( blocks_indexed % 1000 == 0 )
                  {
                      set_db_cache_write_through( true );
                      set_db_cache_write_through( false );
                  }
              };

              if( num_to_id.empty() )
              {
                  for( auto block_itr = block_id_to_data_original.begin(); block_itr.valid(); ++block_itr )
                      insert_block( block_itr.value() );
              }
              else
              {
                  const uint32_t last_known_block_num = num_to_id.crbegin()->first;
                  if( last_known_block_num > BTS_BLOCKCHAIN_MAX_UNDO_HISTORY )
                      my->_min_undo_block = last_known_block_num - BTS_BLOCKCHAIN_MAX_UNDO_HISTORY;

                  for( const auto& num_id : num_to_id )
                  {
                      const auto oblock = block_id_to_data_original.fetch_optional( num_id.second );
                      if( oblock.valid() ) insert_block(*oblock);
                  }
              }

              // Re-enable flushing on all cached databases we disabled it on above
              toggle_leveldb( true );
              set_db_cache_write_through( true );

              block_id_to_data_original.close();
              fc::remove_all( data_dir / "raw_chain/block_id_to_data_original" );

              const size_t final_size = fc::directory_size( data_dir / "raw_chain/block_id_to_block_data_db" );

              std::cout << "\rSuccessfully replayed " << blocks_indexed << " blocks in "
                        << (blockchain::now() - start_time).to_seconds() << " seconds.                          "
                                                                            "\nBlockchain size changed from "
                        << original_size / 1024 / 1024 << "MiB to "
                        << final_size / 1024 / 1024 << "MiB.\n" << std::flush;
          }

          // Process the pending transactions to cache by fees
          for( auto pending_itr = my->_pending_transaction_db.begin(); pending_itr.valid(); ++pending_itr )
          {
             try
             {
                const auto trx = pending_itr.value();
                const auto trx_id = trx.id();
                const auto eval_state = evaluate_transaction( trx, my->_relay_fee );
                const share_type fees = eval_state->total_base_equivalent_fees_paid;
                my->_pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
                my->_pending_transaction_db.store( trx_id, trx );
             }
             catch( const fc::exception& e )
             {
                wlog( "Error processing pending transaction: ${e}", ("e",e.to_detail_string() ) );
             }
          }
      }
      catch( ... )
      {
          error_opening_database = std::current_exception();
      }

      if( error_opening_database )
      {
          elog( "Error opening database!" );
          close();
          fc::remove_all( data_dir / "index" );
          std::rethrow_exception( error_opening_database );
      }
   } FC_CAPTURE_AND_RETHROW( (data_dir) ) }

   void chain_database::close()
   { try {
      my->_pending_transaction_db.close();

      my->_block_id_to_full_block.close();
      my->_block_id_to_undo_state.close();

      my->_fork_number_db.close();
      my->_fork_db.close();

      my->_revalidatable_future_blocks_db.close();

      my->_block_num_to_id_db.close();

      my->_block_id_to_block_record_db.close();

      my->_property_id_to_record.close();

      my->_account_id_to_record.close();
      my->_account_name_to_id.close();
      my->_account_address_to_id.close();

      my->_asset_id_to_record.close();
      my->_asset_symbol_to_id.close();

      my->_slate_id_to_record.close();

      my->_balance_id_to_record.close();

      my->_transaction_id_to_record.close();
      my->_address_to_transaction_ids.close();

      my->_burn_index_to_record.close();

      my->_status_index_to_record.close();

      my->_feed_index_to_record.close();

      my->_ask_db.close();
      my->_bid_db.close();
      my->_short_db.close();
      my->_collateral_db.close();

      my->_market_history_db.close();
      my->_market_transactions_db.close();

      my->_slot_index_to_record.close();
      my->_slot_timestamp_to_delegate.close();
   } FC_CAPTURE_AND_RETHROW() }

   account_record chain_database::get_delegate_record_for_signee( const public_key_type& block_signee )const
   {
      auto delegate_record = get_account_record( address( block_signee ) );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
      return *delegate_record;
   }

   account_record chain_database::get_block_signee( const block_id_type& block_id )const
   {
      auto block_header = get_block_header( block_id );
      auto delegate_record = get_account_record( address( block_header.signee() ) );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
      return *delegate_record;
   }

   account_record chain_database::get_block_signee( uint32_t block_num )const
   {
      return get_block_signee( get_block_id( block_num ) );
   }

   account_record chain_database::get_slot_signee( const time_point_sec timestamp,
                                                   const std::vector<account_id_type>& ordered_delegates )const
   { try {
      auto slot_number = blockchain::get_slot_number( timestamp );
      auto delegate_pos = slot_number % BTS_BLOCKCHAIN_NUM_DELEGATES;
      FC_ASSERT( delegate_pos < ordered_delegates.size() );
      auto delegate_id = ordered_delegates[ delegate_pos ];
      auto delegate_record = get_account_record( delegate_id );
      FC_ASSERT( delegate_record.valid() );
      FC_ASSERT( delegate_record->is_delegate() );
      return *delegate_record;
   } FC_CAPTURE_AND_RETHROW( (timestamp)(ordered_delegates) ) }

   optional<time_point_sec> chain_database::get_next_producible_block_timestamp( const vector<account_id_type>& delegate_ids )const
   { try {
      auto next_block_time = blockchain::get_slot_start_time( blockchain::now() );
      if( next_block_time <= now() ) next_block_time += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      auto last_block_time = next_block_time + (BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

      auto active_delegates = get_active_delegates();
      for( ; next_block_time < last_block_time; next_block_time += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC )
      {
          auto slot_number = blockchain::get_slot_number( next_block_time );
          auto delegate_pos = slot_number % BTS_BLOCKCHAIN_NUM_DELEGATES;
          FC_ASSERT( delegate_pos < active_delegates.size() );
          auto delegate_id = active_delegates[ delegate_pos ];

          if( std::find( delegate_ids.begin(), delegate_ids.end(), delegate_id ) != delegate_ids.end() )
              return next_block_time;
      }
      return optional<time_point_sec>();
   } FC_CAPTURE_AND_RETHROW( (delegate_ids) ) }

   transaction_evaluation_state_ptr chain_database::evaluate_transaction( const signed_transaction& trx,
                                                                          const share_type required_fees )
   { try {
      if( !my->_pending_trx_state )
         my->_pending_trx_state = std::make_shared<pending_chain_state>( shared_from_this() );

      pending_chain_state_ptr          pend_state = std::make_shared<pending_chain_state>(my->_pending_trx_state);
      transaction_evaluation_state_ptr trx_eval_state = std::make_shared<transaction_evaluation_state>( pend_state );

      trx_eval_state->evaluate( trx );
      const share_type fees = trx_eval_state->total_base_equivalent_fees_paid;
      if( fees < required_fees )
      {
          ilog("Transaction ${id} needed relay fee ${required_fees} but only had ${fees}", ("id", trx.id())("required_fees",required_fees)("fees",fees));
          FC_CAPTURE_AND_THROW( insufficient_relay_fee, (fees)(required_fees) );
      }
      // apply changes from this transaction to _pending_trx_state
      pend_state->apply_changes();

      return trx_eval_state;
   } FC_CAPTURE_AND_RETHROW( (trx) ) }

   optional<fc::exception> chain_database::get_transaction_error( const signed_transaction& transaction, const share_type min_fee )
   { try {
       try
       {
          auto pending_state = std::make_shared<pending_chain_state>( shared_from_this() );
          transaction_evaluation_state_ptr eval_state = std::make_shared<transaction_evaluation_state>( pending_state );

          eval_state->evaluate( transaction );
          const share_type fees = eval_state->total_base_equivalent_fees_paid;
          if( fees < min_fee )
             FC_CAPTURE_AND_THROW( insufficient_relay_fee, (fees)(min_fee) );
       }
       catch (const fc::canceled_exception&)
       {
         throw;
       }
       catch( const fc::exception& e )
       {
           return e;
       }
       return optional<fc::exception>();
   } FC_CAPTURE_AND_RETHROW( (transaction) ) }

   signed_block_header chain_database::get_block_header( const block_id_type& block_id )const
   { try {
       return get_block( block_id );
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   signed_block_header chain_database::get_block_header( uint32_t block_num )const
   { try {
       return get_block_header( get_block_id( block_num ) );
   } FC_CAPTURE_AND_RETHROW( (block_num) ) }

   oblock_record chain_database::get_block_record( const block_id_type& block_id ) const
   { try {
       oblock_record record = my->_block_id_to_block_record_db.fetch_optional( block_id );
       if( !record.valid() )
       {
           try
           {
               const full_block block_data = get_block( block_id );
               record = block_record();
               digest_block& temp = *record;
               temp = digest_block( block_data );
               record->id = block_id;
               record->block_size = block_data.block_size();
           }
           catch( const fc::exception& )
           {
           }
       }
       return record;
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   oblock_record chain_database::get_block_record( uint32_t block_num ) const
   { try {
       return get_block_record( get_block_id( block_num ) );
   } FC_CAPTURE_AND_RETHROW( (block_num) ) }

   block_id_type chain_database::get_block_id( uint32_t block_num ) const
   { try {
       return my->_block_num_to_id_db.fetch( block_num );
   } FC_CAPTURE_AND_RETHROW( (block_num) ) }

   vector<transaction_record> chain_database::get_transactions_for_block( const block_id_type& block_id )const
   { try {
       const full_block block = get_block( block_id );

       vector<transaction_record> records;
       records.reserve( block.user_transactions.size() );

       for( const signed_transaction& transaction : block.user_transactions )
           records.push_back( my->_transaction_id_to_record.fetch( transaction.id() ) );

       return records;
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   digest_block chain_database::get_block_digest( const block_id_type& block_id )const
   { try {
       return digest_block( get_block( block_id ) );
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   digest_block chain_database::get_block_digest( uint32_t block_num )const
   { try {
       return get_block_digest( get_block_id( block_num ) );
   } FC_CAPTURE_AND_RETHROW( (block_num) ) }

   full_block chain_database::get_block( const block_id_type& block_id )const
   { try {
       return my->_block_id_to_full_block.fetch( block_id );
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   full_block chain_database::get_block( uint32_t block_num )const
   { try {
       return get_block( get_block_id( block_num ) );
   } FC_CAPTURE_AND_RETHROW( (block_num) ) }

   signed_block_header chain_database::get_head_block()const
   { try {
       return my->_head_block_header;
   } FC_CAPTURE_AND_RETHROW() }

   uint32_t chain_database::find_block_num(fc::time_point_sec &time)const
   { try {
       uint32_t start = 1, end = get_head_block_num();
       auto start_block_time = get_block_header( start ).timestamp;
       if( start_block_time >= time ) {
           return start;
       }
       auto end_block_time = get_block_header( end ).timestamp;
       if( end_block_time <= time ) {
           return end;
       }
       while( end > start + 1 ) {
           double relative = double(time.sec_since_epoch() - start_block_time.sec_since_epoch())
                             / (end_block_time.sec_since_epoch() - start_block_time.sec_since_epoch());
           uint32_t mid = start + ((end - start) * relative);
           if( mid == start ) { mid++; }
           auto mid_block_time = get_block_header( mid ).timestamp;
           if( mid_block_time > time )
           {
               end = mid;
               end_block_time = mid_block_time;
           }
           else
           {
               start = mid;
               start_block_time = mid_block_time;
           }
       }
       return start;
   } FC_CAPTURE_AND_RETHROW( (time) ) }

   /**
    *  Adds the block to the database and manages any reorganizations as a result.
    *
    *  Returns the block_fork_data of the new block, not necessarily the head block
    */
   block_fork_data chain_database::push_block( const full_block& block_data )
   { try {
      uint32_t head_block_num = get_head_block_num();
      if( head_block_num > BTS_BLOCKCHAIN_MAX_UNDO_HISTORY &&
          block_data.block_num <= (head_block_num - BTS_BLOCKCHAIN_MAX_UNDO_HISTORY) )
      {
        elog( "block ${new_block_hash} (number ${new_block_num}) is on a fork older than "
               "our undo history would allow us to switch to (current head block is number ${head_block_num}, undo history is ${undo_history})",
                           ("new_block_hash", block_data.id())("new_block_num", block_data.block_num)
                           ("head_block_num", head_block_num)("undo_history", BTS_BLOCKCHAIN_MAX_UNDO_HISTORY));

        FC_THROW_EXCEPTION(block_older_than_undo_history,
                           "block ${new_block_hash} (number ${new_block_num}) is on a fork older than "
                           "our undo history would allow us to switch to (current head block is number ${head_block_num}, undo history is ${undo_history})",
                           ("new_block_hash", block_data.id())("new_block_num", block_data.block_num)
                           ("head_block_num", head_block_num)("undo_history", BTS_BLOCKCHAIN_MAX_UNDO_HISTORY));
      }

      // only allow a single fiber attempt to push blocks at any given time,
      // this method is not re-entrant.
      fc::unique_lock<fc::mutex> lock( my->_push_block_mutex );

      // The above check probably isn't enough.  We need to make certain that
      // no other code sees the chain_database in an inconsistent state.
      // The lock above prevents two push_blocks from happening at the same time,
      // but we also need to ensure the wallet, blockchain, delegate, &c. loops don't
      // see partially-applied blocks
      ASSERT_TASK_NOT_PREEMPTED();

      const block_id_type& block_id = block_data.id();
      optional<block_fork_data> fork_data = get_block_fork_data( block_id );
      if( fork_data.valid() && fork_data->is_known ) return *fork_data;

      std::pair<block_id_type, block_fork_data> longest_fork = my->store_and_index( block_id, block_data );
      assert(get_block_fork_data(block_id) && "can't get fork data for a block we just successfully pushed");

      /*
      store_and_index has returned the potential chain with the longest_fork (highest block number other than possible the current head block number)
      if (longest_fork is linked and not known to be invalid and is higher than the current head block number)
        highest_unchecked_block_number = longest_fork blocknumber;
        do
          foreach next_fork_to_try in all blocks at same block number
              if (next_fork_try is linked and not known to be invalid)
                try
                  switch_to_fork(next_fork_to_try) //this throws if block in fork is invalid, then we'll try another fork
                  return
                catch block from future and add to database for potential revalidation on startup or if we get from another peer later
                catch any other invalid block and do nothing
          --highest_unchecked_block_number
        while(highest_unchecked_block_number > 0)
      */
      if (longest_fork.second.can_link())
      {
        full_block longest_fork_block = my->_block_id_to_full_block.fetch(longest_fork.first);
        uint32_t highest_unchecked_block_number = longest_fork_block.block_num;
        if (highest_unchecked_block_number > head_block_num)
        {
          do
          {
            optional<vector<block_id_type>> parallel_blocks = my->_fork_number_db.fetch_optional(highest_unchecked_block_number);
            if (parallel_blocks)
              //for all blocks at same block number
              for (const block_id_type& next_fork_to_try_id : *parallel_blocks)
              {
                block_fork_data next_fork_to_try = my->_fork_db.fetch(next_fork_to_try_id);
                if (next_fork_to_try.can_link())
                  try
                  {
                    my->switch_to_fork(next_fork_to_try_id); //verify this works if next_fork_to_try is current head block
                    return *get_block_fork_data(block_id);
                  }
                  catch (const time_in_future& e)
                  {
                    // Blocks from the future can become valid later, so keep a list of these blocks that we can iterate over
                    // whenever we think our clock time has changed from it's standard flow
                    my->_revalidatable_future_blocks_db.store(block_id, 0);
                    wlog("fork rejected because it has block with time in future, storing block id for revalidation later");
                  }
                  catch (const fc::exception& e) //swallow any invalidation exceptions except for time_in_future invalidations
                  {
                    wlog("fork permanently rejected as it has permanently invalid block: ${x}", ("x",e.to_detail_string()));
                  }
              }
            --highest_unchecked_block_number;
          } while(highest_unchecked_block_number > 0); // while condition should only fail if we've never received a valid block yet
        } //end if fork is longer than current chain (including possibly by extending chain)
      }
      else
      {
         elog( "unable to link longest fork ${f}", ("f", longest_fork) );
      }
      return *get_block_fork_data(block_id);
   } FC_CAPTURE_AND_RETHROW() }

  std::vector<block_id_type> chain_database::get_fork_history( const block_id_type& id )
  {
    return my->get_fork_history(id);
  }

   /** return the timestamp from the head block */
   fc::time_point_sec chain_database::now()const
   {
      if( my->_head_block_header.block_num <= 0 ) /* Genesis */
      {
          auto slot_number = blockchain::get_slot_number( blockchain::now() );
          return blockchain::get_slot_start_time( slot_number - 1 );
      }

      return my->_head_block_header.timestamp;
   }

   asset_id_type chain_database::get_asset_id( const string& symbol )const
   { try {
       auto arec = get_asset_record( symbol );
       FC_ASSERT( arec.valid() );
       return arec->id;
   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

   bool chain_database::is_valid_symbol( const string& symbol )const
   { try {
       return get_asset_record(symbol).valid();
   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

   vector<operation> chain_database::get_recent_operations(operation_type_enum t)const
   {
      const auto& recent_op_queue = my->_recent_operations[t];
      vector<operation> recent_ops(recent_op_queue.size());
      std::copy(recent_op_queue.begin(), recent_op_queue.end(), recent_ops.begin());
      return recent_ops;
   }

   void chain_database::store_recent_operation(const operation& o)
   {
      auto& recent_op_queue = my->_recent_operations[o.type];
      recent_op_queue.push_back(o);
      if( recent_op_queue.size() > MAX_RECENT_OPERATIONS )
        recent_op_queue.pop_front();
   }

   otransaction_record chain_database::get_transaction( const transaction_id_type& trx_id, bool exact )const
   { try {
      auto trx_rec = my->_transaction_id_to_record.fetch_optional( trx_id );
      if( trx_rec || exact )
      {
         if( trx_rec )
            FC_ASSERT( trx_rec->trx.id() == trx_id,"", ("trx_rec->id",trx_rec->trx.id()) );
         return trx_rec;
      }

      auto itr = my->_transaction_id_to_record.lower_bound( trx_id );
      if( itr.valid() )
      {
         auto id = itr.key();

         if( memcmp( (char*)&id, (const char*)&trx_id, 4 ) != 0 )
            return otransaction_record();

         return itr.value();
      }
      return otransaction_record();
   } FC_CAPTURE_AND_RETHROW( (trx_id)(exact) ) }

   void chain_database::store_transaction( const transaction_id_type& record_id,
                                           const transaction_record& record_to_store )
   { try {
       store( record_id, record_to_store );
   } FC_CAPTURE_AND_RETHROW( (record_id)(record_to_store) ) }

   void chain_database::scan_balances( const function<void( const balance_record& )> callback )const
   { try {
       for( auto iter = my->_balance_id_to_record.unordered_begin();
            iter != my->_balance_id_to_record.unordered_end(); ++iter )
       {
           callback( iter->second );
       }
   } FC_CAPTURE_AND_RETHROW() }

   void chain_database::scan_transactions( const function<void( const transaction_record& )> callback )const
   { try {
       for( auto iter = my->_transaction_id_to_record.begin();
            iter.valid(); ++iter )
       {
           callback( iter.value() );
       }
   } FC_CAPTURE_AND_RETHROW() }

   void chain_database::scan_unordered_accounts( const function<void( const account_record& )> callback )const
   { try {
       for( auto iter = my->_account_id_to_record.unordered_begin();
            iter != my->_account_id_to_record.unordered_end(); ++iter )
       {
           callback( iter->second );
       }
   } FC_CAPTURE_AND_RETHROW() }

   void chain_database::scan_ordered_accounts( const function<void( const account_record& )> callback )const
   { try {
       for( auto iter = my->_account_name_to_id.ordered_first(); iter.valid(); ++iter )
       {
           const oaccount_record& record = lookup<account_record>( iter.value() );
           if( record.valid() ) callback( *record );
       }
   } FC_CAPTURE_AND_RETHROW() }

   void chain_database::scan_unordered_assets( const function<void( const asset_record& )> callback )const
   { try {
       for( auto iter = my->_asset_id_to_record.unordered_begin();
            iter != my->_asset_id_to_record.unordered_end(); ++iter )
       {
           callback( iter->second );
       }
   } FC_CAPTURE_AND_RETHROW() }

   void chain_database::scan_ordered_assets( const function<void( const asset_record& )> callback )const
   { try {
       for( auto iter = my->_asset_symbol_to_id.ordered_first(); iter.valid(); ++iter )
       {
           const oasset_record& record = lookup<asset_record>( iter.value() );
           if( record.valid() ) callback( *record );
       }
   } FC_CAPTURE_AND_RETHROW() }

   /** this should throw if the trx is invalid */
   transaction_evaluation_state_ptr chain_database::store_pending_transaction( const signed_transaction& trx, bool override_limits )
   { try {
      auto trx_id = trx.id();
      if (override_limits)
        ilog("storing new local transaction with id ${id}", ("id", trx_id));

      auto current_itr = my->_pending_transaction_db.find( trx_id );
      if( current_itr.valid() )
        return nullptr;

      share_type relay_fee = my->_relay_fee;
      if( !override_limits )
      {
         if( my->_pending_fee_index.size() > BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE )
         {
             auto overage = my->_pending_fee_index.size() - BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE;
             relay_fee = my->_relay_fee * overage * overage;
         }
      }

      transaction_evaluation_state_ptr eval_state = evaluate_transaction( trx, relay_fee );
      const share_type fees = eval_state->total_base_equivalent_fees_paid;

      //if( fees < my->_relay_fee )
      //   FC_CAPTURE_AND_THROW( insufficient_relay_fee, (fees)(my->_relay_fee) );

      my->_pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
      my->_pending_transaction_db.store( trx_id, trx );

      return eval_state;
   } FC_CAPTURE_AND_RETHROW( (trx)(override_limits) ) }

   /** returns all transactions that are valid (independent of each other) sorted by fee */
   std::vector<transaction_evaluation_state_ptr> chain_database::get_pending_transactions()const
   {
      std::vector<transaction_evaluation_state_ptr> trxs;
      for( const auto& item : my->_pending_fee_index )
      {
          trxs.push_back( item.second );
      }
      return trxs;
   }

   full_block chain_database::generate_block( const time_point_sec block_timestamp, const delegate_config& config )
   { try {
      const time_point start_time = time_point::now();
      FC_ASSERT( block_timestamp < (fc::time_point_sec() + fc::seconds( 1444741200 )), "Upgrade to BTS 2.0" );

      const pending_chain_state_ptr pending_state = std::make_shared<pending_chain_state>( shared_from_this() );
      if( pending_state->get_head_block_num() >= BTS_V0_4_9_FORK_BLOCK_NUM )
          my->execute_markets( block_timestamp, pending_state );

      // Initialize block
      full_block new_block;
      size_t block_size = new_block.block_size();
      if( config.block_max_transaction_count > 0 && config.block_max_size > block_size )
      {
          // Evaluate pending transactions
          const vector<transaction_evaluation_state_ptr> pending_trx = get_pending_transactions();
          for( const transaction_evaluation_state_ptr& item : pending_trx )
          {
              // Check block production time limit
              if( time_point::now() - start_time >= config.block_max_production_time )
                  break;

              const signed_transaction& new_transaction = item->trx;
              try
              {
                  // Check transaction size limit
                  const size_t transaction_size = new_transaction.data_size();
                  if( transaction_size > config.transaction_max_size )
                  {
                      wlog( "Excluding transaction ${id} of size ${size} because it exceeds transaction size limit ${limit}",
                            ("id",new_transaction.id())("size",transaction_size)("limit",config.transaction_max_size) );
                      continue;
                  }

                  // Check block size limit
                  if( block_size + transaction_size > config.block_max_size )
                  {
                      wlog( "Excluding transaction ${id} of size ${size} because block would exceed block size limit ${limit}",
                            ("id",new_transaction.id())("size",transaction_size)("limit",config.block_max_size) );
                      continue;
                  }

                  // Check transaction blacklist
                  if( !config.transaction_blacklist.empty() )
                  {
                      const transaction_id_type id = new_transaction.id();
                      if( config.transaction_blacklist.count( id ) > 0 )
                      {
                          wlog( "Excluding blacklisted transaction ${id}", ("id",id) );
                          continue;
                      }
                  }

                  // Check operation blacklist
                  if( !config.operation_blacklist.empty() )
                  {
                      optional<operation_type_enum> blacklisted_op;
                      for( const operation& op : new_transaction.operations )
                      {
                          if( config.operation_blacklist.count( op.type ) > 0 )
                          {
                              blacklisted_op = op.type;
                              break;
                          }
                      }
                      if( blacklisted_op.valid() )
                      {
                          wlog( "Excluding transaction ${id} because of blacklisted operation ${op}",
                                ("id",new_transaction.id())("op",*blacklisted_op) );
                          continue;
                      }
                  }

                  // Validate transaction
                  auto pending_trx_state = std::make_shared<pending_chain_state>( pending_state );
                  {
                      auto trx_eval_state = std::make_shared<transaction_evaluation_state>( pending_trx_state );
                      trx_eval_state->_enforce_canonical_signatures = config.transaction_canonical_signatures_required;
                      trx_eval_state->evaluate( new_transaction );

                      // Check transaction fee limit
                      const share_type transaction_fee = trx_eval_state->total_base_equivalent_fees_paid;
                      if( transaction_fee < config.transaction_min_fee )
                      {
                          wlog( "Excluding transaction ${id} with fee ${fee} because it does not meet transaction fee limit ${limit}",
                                ("id",new_transaction.id())("fee",transaction_fee)("limit",config.transaction_min_fee) );
                          continue;
                      }
                  }

                  // Include transaction
                  pending_trx_state->apply_changes();
                  new_block.user_transactions.push_back( new_transaction );
                  block_size += transaction_size;

                  // Check block transaction count limit
                  if( new_block.user_transactions.size() >= config.block_max_transaction_count )
                      break;
              }
              catch( const fc::canceled_exception& )
              {
                  throw;
              }
              catch( const fc::exception& e )
              {
                  wlog( "Pending transaction was found to be invalid in context of block\n${trx}\n${e}",
                        ("trx",fc::json::to_pretty_string( new_transaction ))("e",e.to_detail_string()) );
              }
          }
      }

      const signed_block_header head_block = get_head_block();

      // Populate block header
      new_block.previous            = head_block.block_num > 0 ? head_block.id() : block_id_type();
      new_block.block_num           = head_block.block_num + 1;
      new_block.timestamp           = block_timestamp;
      new_block.transaction_digest  = digest_block( new_block ).calculate_transaction_digest();

      return new_block;
   } FC_CAPTURE_AND_RETHROW( (block_timestamp)(config) ) }

   void chain_database::add_observer( chain_observer* observer )
   {
      my->_observers.insert(observer);
   }

   void chain_database::remove_observer( chain_observer* observer )
   {
      my->_observers.erase(observer);
   }

   bool chain_database::is_known_block( const block_id_type& block_id )const
   { try {
      auto fork_data = get_block_fork_data( block_id );
      return fork_data && fork_data->is_known;
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   bool chain_database::is_included_block( const block_id_type& block_id )const
   { try {
      auto fork_data = get_block_fork_data( block_id );
      return fork_data && fork_data->is_included;
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   optional<block_fork_data> chain_database::get_block_fork_data( const block_id_type& block_id )const
   { try {
      return my->_fork_db.fetch_optional( block_id );
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   uint32_t chain_database::get_block_num( const block_id_type& block_id )const
   { try {
       if( block_id == block_id_type() ) return 0;
       return get_block( block_id ).block_num;
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   uint32_t chain_database::get_head_block_num()const
   { try {
      return my->_head_block_header.block_num;
   } FC_CAPTURE_AND_RETHROW() }

   block_id_type chain_database::get_head_block_id()const
   { try {
      return my->_head_block_id;
   } FC_CAPTURE_AND_RETHROW() }

   unordered_map<balance_id_type, balance_record> chain_database::get_balances_for_address( const address& addr )const
   { try {
        unordered_map<balance_id_type, balance_record> records;
        const auto scan_balance = [ &addr, &records ]( const balance_record& record )
        {
            if( record.is_owner( addr ) )
                records[ record.id() ] = record;
        };
        scan_balances( scan_balance );
        return records;
   } FC_CAPTURE_AND_RETHROW( (addr) ) }

   unordered_map<balance_id_type, balance_record> chain_database::get_balances_for_key( const public_key_type& key )const
   { try {
        unordered_map<balance_id_type, balance_record> records;
        const vector<address> addrs
        {
            address( key ),
            address( pts_address( key, false, 56 ) ),
            address( pts_address( key, true, 56 ) ),
            address( pts_address( key, false, 0 ) ),
            address( pts_address( key, true, 0 ) )
        };
        const auto scan_balance = [ &addrs, &records ]( const balance_record& record )
        {
            const auto& owner = record.condition.owner();
            if( !owner.valid() )
                return;

            for( const address& addr : addrs )
            {
                if( addr == *owner )
                {
                    records[ record.id() ] = record;
                    return;
                }
            }
        };
        scan_balances( scan_balance );
        return records;
   } FC_CAPTURE_AND_RETHROW( (key) ) }

    vector<account_record> chain_database::get_accounts( const string& first, uint32_t limit )const
    { try {
        vector<account_record> records;
        records.reserve( std::min( size_t( limit ), my->_account_name_to_id.size() ) );
        for( auto iter = my->_account_name_to_id.ordered_lower_bound( first ); iter.valid(); ++iter )
        {
            const oaccount_record& record = lookup<account_record>( iter.value() );
            if( record.valid() ) records.push_back( *record );
            if( records.size() >= limit ) break;
        }
        return records;
    } FC_CAPTURE_AND_RETHROW( (first)(limit) ) }

    vector<asset_record> chain_database::get_assets( const string& first, uint32_t limit )const
    { try {
        vector<asset_record> records;
        records.reserve( std::min( size_t( limit ), my->_asset_symbol_to_id.size() ) );
        for( auto iter = my->_asset_symbol_to_id.ordered_lower_bound( first ); iter.valid(); ++iter )
        {
            const oasset_record& record = lookup<asset_record>( iter.value() );
            if( record.valid() ) records.push_back( *record );
            if( records.size() >= limit ) break;
        }
        return records;
    } FC_CAPTURE_AND_RETHROW( (first)(limit) ) }

    string chain_database::export_fork_graph( uint32_t start_block, uint32_t end_block, const fc::path& filename )const
    {
      FC_ASSERT( start_block >= 0 );
      FC_ASSERT( end_block >= start_block );
      std::stringstream out;
      out << "digraph G { \n";
      out << "rankdir=LR;\n";

      bool first = true;
      fc::time_point_sec start_time;
      std::map<uint32_t, vector<signed_block_header>> nodes_by_rank;
      //std::set<uint32_t> ranks_in_use;
      for( auto block_itr = my->_block_id_to_full_block.begin(); block_itr.valid(); ++block_itr )
      {
        const full_block& block = block_itr.value();
        if (first)
        {
          first = false;
          start_time = block.timestamp;
        }
        std::cout << block.block_num << "  start " << start_block << "  end " << end_block << "\n";
        if ( block.block_num >= start_block && block.block_num <= end_block )
        {
          unsigned rank = (unsigned)((block.timestamp - start_time).to_seconds() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

          //ilog( "${id} => ${r}", ("id",fork_itr.key())("r",fork_data) );
          nodes_by_rank[rank].push_back(block);
        }
      }

      for( const auto& item : nodes_by_rank )
      {
        out << "{rank=same l" << item.first << "[style=invis, shape=point] ";
        for( const auto& record : item.second )
          out << "; \"" << std::string(record.id()) << "\"";
        out << ";}\n";
      }
      for( const auto& blocks_at_time : nodes_by_rank )
      {
        for( const auto& block : blocks_at_time.second )
        {
          auto delegate_record = get_block_signee( block.id() );

          out << '"' << std::string ( block.id() ) <<"\" "
              << "[label=<"
              << std::string ( block.id() ).substr(0,5)
              << "<br/>" << blocks_at_time.first
              << "<br/>" << block.block_num
              << "<br/>" << delegate_record.name
              << ">,style=filled,rank=" << blocks_at_time.first << "];\n";
          out << '"' << std::string ( block.id() ) <<"\" -> \"" << std::string( block.previous ) << "\";\n";
        }
      }
      out << "edge[style=invis];\n";

      bool first2 = true;
      for( const auto& item : nodes_by_rank )
      {
        if (first2)
          first2 = false;
        else
          out << "->";
        out << "l" << item.first;
      }
      out << ";\n";
      out << "}";

      if( filename == "" )
          return out.str();

      FC_ASSERT( !fc::exists( fc::path( filename ) ) );
      std::ofstream fileout( filename.generic_string().c_str() );
      fileout << out.str();

      return std::string();
    }

    std::map<uint32_t, std::vector<fork_record>> chain_database::get_forks_list()const
    {
        std::map<uint32_t, std::vector<fork_record>> fork_blocks;
        for( auto iter = my->_fork_db.begin(); iter.valid(); ++iter )
        {
            try
            {
                auto fork_iter = iter.value();
                if( fork_iter.next_blocks.size() > 1 )
                {
                    vector<fork_record> forks;

                    for( const auto& forked_block_id : fork_iter.next_blocks )
                    {
                        fork_record fork;
                        block_fork_data fork_data = my->_fork_db.fetch(forked_block_id);

                        fork.block_id = forked_block_id;
                        fork.signing_delegate = get_block_signee( forked_block_id ).id;
                        fork.is_valid = fork_data.is_valid;
                        fork.invalid_reason = fork_data.invalid_reason;
                        fork.is_current_fork = fork_data.is_included;

                        if( get_statistics_enabled() )
                        {
                            const oblock_record& record = my->_block_id_to_block_record_db.fetch_optional( forked_block_id );
                            if( record.valid() )
                            {
                                fork.latency = record->latency;
                                fork.transaction_count = record->user_transaction_ids.size();
                                fork.size = record->block_size;
                                fork.timestamp = record->timestamp;
                            }
                        }

                        forks.push_back(fork);
                    }

                    fork_blocks[get_block_num( iter.key() )] = forks;
                }
            }
            catch( const fc::exception& )
            {
                wlog( "error fetching block num of block ${b} while building fork list", ("b",iter.key()));
                throw;
            }
        }

        return fork_blocks;
    }

    vector<slot_record> chain_database::get_delegate_slot_records( const account_id_type delegate_id, uint32_t limit )const
    { try {
        FC_ASSERT( limit > 0 );

        vector<slot_record> slot_records;
        slot_records.reserve( std::min( limit, get_head_block_num() ) );

        const slot_index key = slot_index( delegate_id, my->_head_block_header.timestamp );
        for( auto iter = my->_slot_index_to_record.lower_bound( key ); iter.valid(); ++iter )
        {
            const slot_record& record = iter.value();
            if( record.index.delegate_id != delegate_id ) break;
            slot_records.push_back( record );
            if( slot_records.size() >= limit ) break;
        }

        return slot_records;
    } FC_CAPTURE_AND_RETHROW( (delegate_id)(limit) ) }

   oorder_record chain_database::get_bid_record( const market_index_key&  key )const
   {
      return my->_bid_db.fetch_optional(key);
   }

   omarket_order chain_database::get_lowest_ask_record( const asset_id_type quote_id, const asset_id_type base_id )
   {
      omarket_order result;
      auto itr = my->_ask_db.lower_bound( market_index_key( price( 0, quote_id, base_id ) ) );
      if( itr.valid() )
      {
         auto market_index = itr.key();
         if( market_index.order_price.quote_asset_id == quote_id &&
             market_index.order_price.base_asset_id == base_id      )
            return market_order( ask_order, market_index, itr.value() );
      }
      return result;
   }

   oorder_record chain_database::get_ask_record( const market_index_key&  key )const
   {
      return my->_ask_db.fetch_optional(key);
   }

   oorder_record chain_database::get_short_record( const market_index_key& key )const
   {
      return my->_short_db.fetch_optional(key);
   }

   ocollateral_record chain_database::get_collateral_record( const market_index_key& key )const
   {
      return my->_collateral_db.fetch_optional(key);
   }

   void chain_database::store_bid_record( const market_index_key& key, const order_record& order )
   {
      if( order.is_null() )
         my->_bid_db.remove( key );
      else
         my->_bid_db.store( key, order );
   }

   void chain_database::store_ask_record( const market_index_key& key, const order_record& order )
   {
      if( order.is_null() )
         my->_ask_db.remove( key );
      else
         my->_ask_db.store( key, order );
   }

   void chain_database::store_short_record( const market_index_key& key, const order_record& order )
   {
      if( order.is_null() )
      {
         auto existing = my->_short_db.fetch_optional( key );
         if( existing )
            my->_short_db.remove( key );
      }
      else
      {
         my->_short_db.store( key, order );
      }
   }

   void chain_database::store_collateral_record( const market_index_key& key, const collateral_record& collateral )
   {
      if( collateral.is_null() )
      {
         auto old_record = my->_collateral_db.fetch_optional(key);
         if( old_record && old_record->expiration != collateral.expiration)
         {
            my->_collateral_expiration_index.erase( {key.order_price.quote_asset_id,  old_record->expiration, key } );
         }
         my->_collateral_db.remove( key );
      }
      else
      {
         auto old_record = my->_collateral_db.fetch_optional(key);
         if( old_record && old_record->expiration != collateral.expiration)
         {
            my->_collateral_expiration_index.erase( {key.order_price.quote_asset_id,  old_record->expiration, key } );
         }
         my->_collateral_expiration_index.insert( {key.order_price.quote_asset_id, collateral.expiration, key } );
         my->_collateral_db.store( key, collateral );
      }
   }

   string chain_database::get_asset_symbol( const asset_id_type asset_id )const
   { try {
      auto asset_rec = get_asset_record( asset_id );
      FC_ASSERT( asset_rec.valid(), "Unknown Asset ID: ${id}", ("asset_id",asset_id) );
      return asset_rec->symbol;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("asset_id",asset_id) ) }

   /**
    *   Calculates the percentage of blocks produced in the last 10 rounds as an average
    *   measure of the delegate participation rate.
    *
    *   @return a value betwee 0 and 100
    */
   double chain_database::get_average_delegate_participation()const
   { try {
      const auto head_num = get_head_block_num();
      const auto now = bts::blockchain::now();
      if( head_num < 1 )
      {
          return 0;
      }
      else if( head_num <= BTS_BLOCKCHAIN_NUM_DELEGATES )
      {
         // what percent of the maximum total blocks that could have been produced
         // have been produced.
         const auto expected_blocks = (now - get_block_header( 1 ).timestamp).to_seconds() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
         return 100*double( head_num ) / expected_blocks;
      }
      else
      {
         // if 10*N blocks ago is longer than 10*N*INTERVAL_SEC ago then we missed blocks, calculate
         // in terms of percentage time rather than percentage blocks.
         const auto starting_time = get_block_header( head_num - BTS_BLOCKCHAIN_NUM_DELEGATES ).timestamp;
         const auto expected_production = (now - starting_time).to_seconds() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
         return 100*double( BTS_BLOCKCHAIN_NUM_DELEGATES ) / expected_production;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   optional<market_order> chain_database::get_market_bid( const market_index_key& key )const
   { try {
       { // absolute bids
          auto market_itr  = my->_bid_db.find(key);
          if( market_itr.valid() )
             return market_order { bid_order, market_itr.key(), market_itr.value() };
       }
       return optional<market_order>();
   } FC_CAPTURE_AND_RETHROW( (key) ) }

   vector<market_order> chain_database::get_market_bids( const string& quote_symbol, const string& base_symbol, uint32_t limit )const
   { try {
       auto quote_id = get_asset_id( quote_symbol );
       auto base_id  = get_asset_id( base_symbol );
       if( base_id >= quote_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quote_id)(base_id) );

       vector<market_order> results;

       //We dance around like this because the _bid_db sorts the bids backwards, so we must iterate it backwards.
       { // absolute bids
          const price next_pair = (base_id+1 == quote_id) ? price( 0, quote_id+1, 0 ) : price( 0, quote_id, base_id+1 );
          auto market_itr = my->_bid_db.lower_bound( market_index_key( next_pair ) );
          if( market_itr.valid() )   --market_itr;
          else market_itr = my->_bid_db.last();

          while( market_itr.valid() )
          {
             auto key = market_itr.key();
             if( key.order_price.quote_asset_id == quote_id &&
                 key.order_price.base_asset_id == base_id  )
             {
                results.push_back( {bid_order, key, market_itr.value()} );
             }
             else break;


             if( results.size() >= limit )
                return results;

             --market_itr;
          }
       }
       return results;
   } FC_CAPTURE_AND_RETHROW( (quote_symbol)(base_symbol)(limit) ) }

   optional<market_order> chain_database::get_market_short( const market_index_key& key )const
   { try {
       auto market_itr  = my->_short_db.find(key);
       if( market_itr.valid() )
          return market_order { short_order, market_itr.key(), market_itr.value() };

       return optional<market_order>();
   } FC_CAPTURE_AND_RETHROW( (key) ) }

   vector<market_order> chain_database::get_market_shorts( const string& quote_symbol, uint32_t limit )const
   { try {
       asset_id_type quote_id = get_asset_id( quote_symbol );
       asset_id_type base_id  = 0;
       if( base_id >= quote_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quote_id)(base_id) );

       vector<market_order> results;
       //We dance around like this because the database sorts the shorts backwards, so we must iterate it backwards.
       const price next_pair = (base_id+1 == quote_id) ? price( 0, quote_id+1, 0 ) : price( 0, quote_id, base_id+1 );
       auto market_itr = my->_short_db.lower_bound( market_index_key( next_pair ) );
       if( market_itr.valid() )   --market_itr;
       else market_itr = my->_short_db.last();

       while( market_itr.valid() )
       {
          auto key = market_itr.key();
          if( key.order_price.quote_asset_id == quote_id &&
              key.order_price.base_asset_id == base_id  )
          {
             order_record value = market_itr.value();
             results.push_back( {short_order, key, value, value.balance, key.order_price} );
          }
          else
          {
             break;
          }

          if( results.size() >= limit )
             return results;

          --market_itr;
       }
       return results;
   } FC_CAPTURE_AND_RETHROW( (quote_symbol)(limit) ) }

   vector<market_order> chain_database::get_market_covers( const string& quote_symbol, const string& base_symbol, uint32_t limit )const
   { try {
       asset_id_type quote_asset_id = get_asset_id( quote_symbol );
       asset_id_type base_asset_id = get_asset_id( base_symbol );
       if( base_asset_id != 0 )
          return vector<market_order>();

       if( base_asset_id >= quote_asset_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quote_asset_id)(base_asset_id) );

       vector<market_order> results;

       auto market_itr  = my->_collateral_db.lower_bound( market_index_key( price( 0, quote_asset_id, base_asset_id ) ) );
       while( market_itr.valid() )
       {
          auto key = market_itr.key();
          if( key.order_price.quote_asset_id == quote_asset_id &&
              key.order_price.base_asset_id == base_asset_id  )
          {
             auto collat_record = market_itr.value();
             results.push_back( {cover_order,
                                 key,
                                 order_record(collat_record.payoff_balance),
                                 collat_record.collateral_balance,
                                 collat_record.interest_rate,
                                 collat_record.expiration } );
          }
          else
          {
             break;
          }

          if( results.size() >= limit )
             return results;

          ++market_itr;
       }
       return results;
   } FC_CAPTURE_AND_RETHROW( (quote_symbol)(limit) ) }

   optional<market_order> chain_database::get_market_ask( const market_index_key& key )const
   { try {
       { // abs asks
          auto market_itr  = my->_ask_db.find(key);
          if( market_itr.valid() )
             return market_order { ask_order, market_itr.key(), market_itr.value() };
       }
       return optional<market_order>();
   } FC_CAPTURE_AND_RETHROW( (key) ) }


   share_type           chain_database::get_asset_collateral( const string& symbol )
   { try {
       asset_id_type quote_asset_id = get_asset_id( symbol);
       asset_id_type base_asset_id = 0;
       auto total = share_type(0);

       auto market_itr = my->_collateral_db.lower_bound( market_index_key( price( 0, quote_asset_id, base_asset_id ) ) );
       while( market_itr.valid() )
       {
           auto key = market_itr.key();
           if( key.order_price.quote_asset_id == quote_asset_id
               &&  key.order_price.base_asset_id == base_asset_id )
           {
               total += market_itr.value().collateral_balance;
           }
           else
           {
               break;
           }

           market_itr++;
       }
       return total;

   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

   vector<market_order> chain_database::get_market_asks( const string& quote_symbol, const string& base_symbol, uint32_t limit )const

   { try {
       auto quote_asset_id = get_asset_id( quote_symbol );
       auto base_asset_id  = get_asset_id( base_symbol );
       if( base_asset_id >= quote_asset_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quote_asset_id)(base_asset_id) );

       vector<market_order> results;
       { // absolute asks
          auto market_itr  = my->_ask_db.lower_bound( market_index_key( price( 0, quote_asset_id, base_asset_id ) ) );
          while( market_itr.valid() )
          {
             auto key = market_itr.key();
             if( key.order_price.quote_asset_id == quote_asset_id &&
                 key.order_price.base_asset_id == base_asset_id  )
             {
                results.push_back( {ask_order, key, market_itr.value()} );
             }
             else
             {
                break;
             }

             if( results.size() >= limit )
                return results;

             ++market_itr;
          }
       }
       return results;
   } FC_CAPTURE_AND_RETHROW( (quote_symbol)(base_symbol)(limit) ) }

   vector<market_order> chain_database::scan_market_orders( std::function<bool( const market_order& )> filter,
                                                            uint32_t limit, order_type_enum type )const
   { try {
       vector<market_order> orders;
       if( limit == 0 ) return orders;

       if( type == null_order || type == ask_order )
       {
           for( auto itr = my->_ask_db.begin(); itr.valid(); ++itr )
           {
               const auto order = market_order( ask_order, itr.key(), itr.value() );
               if( filter( order ) )
               {
                   orders.push_back( order );
                   if( orders.size() >= limit )
                       return orders;
               }
           }
       }

       if( type == null_order || type == bid_order )
       {
           for( auto itr = my->_bid_db.begin(); itr.valid(); ++itr )
           {
               const auto order = market_order( bid_order, itr.key(), itr.value() );
               if( filter( order ) )
               {
                   orders.push_back( order );
                   if( orders.size() >= limit )
                       return orders;
               }
           }
       }

       if( type == null_order || type == short_order )
       {
           for( auto itr = my->_short_db.begin(); itr.valid(); ++itr )
           {
               const market_index_key& key = itr.key();
               const order_record& record = itr.value();
               const auto order = market_order( short_order, key, record, record.balance, key.order_price );
               if( filter( order ) )
               {
                   orders.push_back( order );
                   if( orders.size() >= limit )
                       return orders;
               }
           }
       }

       if( type == null_order || type == cover_order ) {
           for( auto itr = my->_collateral_db.begin(); itr.valid(); ++itr )
           {
               const auto collateral_rec = itr.value();
               const auto order_rec = order_record( collateral_rec.payoff_balance );
               const auto order = market_order( cover_order,
                                                itr.key(),
                                                order_rec,
                                                collateral_rec.collateral_balance,
                                                collateral_rec.interest_rate,
                                                collateral_rec.expiration );
               if( filter( order ) )
               {
                   orders.push_back( order );
                   if( orders.size() >= limit )
                       return orders;
               }
           }
       }

       return orders;
   } FC_CAPTURE_AND_RETHROW() }

   optional<market_order> chain_database::get_market_order( const order_id_type& order_id, order_type_enum type )const
   { try {
       const auto filter = [&]( const market_order& order ) -> bool
       {
           return order.get_id() == order_id;
       };

       const auto orders = scan_market_orders( filter, 1, type );
       if( !orders.empty() )
           return orders.front();

       return optional<market_order>();
   } FC_CAPTURE_AND_RETHROW() }

   pending_chain_state_ptr chain_database::get_pending_state()const
   {
      return my->_pending_trx_state;
   }

   void chain_database::store_market_history_record(const market_history_key& key, const market_history_record& record)
   {
     if( record.quote_volume == 0 && record.base_volume == 0 )
       my->_market_history_db.remove( key );
     else
       my->_market_history_db.store( key, record );
   }

   omarket_history_record chain_database::get_market_history_record(const market_history_key& key) const
   {
     return my->_market_history_db.fetch_optional( key );
   }

   vector<pair<asset_id_type, asset_id_type>> chain_database::get_market_pairs()const
   {
       vector<pair<asset_id_type, asset_id_type>> pairs;
       for( auto iter = my->_status_index_to_record.begin(); iter.valid(); ++iter )
       {
           const status_index& index = iter.key();
           pairs.push_back( std::make_pair( index.quote_id, index.base_id ) );
       }
       return pairs;
   }

   market_history_points chain_database::get_market_price_history( const asset_id_type quote_id,
                                                                   const asset_id_type base_id,
                                                                   const fc::time_point start_time,
                                                                   const fc::microseconds duration,
                                                                   market_history_key::time_granularity_enum granularity )const
   {
      time_point_sec end_time = start_time + duration;
      auto record_itr = my->_market_history_db.lower_bound( market_history_key(quote_id, base_id, granularity, start_time) );
      market_history_points history;
      auto base = get_asset_record(base_id);
      auto quote = get_asset_record(quote_id);

      FC_ASSERT( base && quote );

      while( record_itr.valid()
             && record_itr.key().quote_id == quote_id
             && record_itr.key().base_id == base_id
             && record_itr.key().granularity == granularity
             && record_itr.key().timestamp <= end_time )
      {
        history.push_back( {
                             record_itr.key().timestamp,
                             to_pretty_price( record_itr.value().highest_bid, false ),
                             to_pretty_price( record_itr.value().lowest_ask, false ),
                             to_pretty_price( record_itr.value().opening_price, false ),
                             to_pretty_price( record_itr.value().closing_price, false ),
                             record_itr.value().base_volume,
                             record_itr.value().quote_volume
                           } );
        ++record_itr;
      }

      return history;
   }

   bool chain_database::is_known_transaction( const transaction& trx )const
   { try {
       return my->_unique_transactions.count( unique_transaction_key( trx, get_chain_id() ) ) > 0;
   } FC_CAPTURE_AND_RETHROW( (trx) ) }

   void chain_database::set_relay_fee( share_type shares )
   {
      my->_relay_fee = shares;
   }

   share_type chain_database::get_relay_fee()
   {
      return my->_relay_fee;
   }

   void chain_database::set_market_transactions( vector<market_transaction> trxs )
   {
      if( trxs.size() == 0 )
      {
         my->_market_transactions_db.remove( get_head_block_num()+1 );
      }
      else
      {
         my->_market_transactions_db.store( get_head_block_num()+1, trxs );
      }
   }

   vector<market_transaction> chain_database::get_market_transactions( const uint32_t block_num )const
   { try {
       const auto oresult = my->_market_transactions_db.fetch_optional( block_num );
       if( oresult.valid() ) return *oresult;
       return vector<market_transaction>();
   } FC_CAPTURE_AND_RETHROW( (block_num) ) }

   vector<order_history_record> chain_database::market_order_history(asset_id_type quote,
                                                                     asset_id_type base,
                                                                     uint32_t skip_count,
                                                                     uint32_t limit,
                                                                     const address& owner)
   {
      FC_ASSERT(limit <= 10000, "Limit must be at most 10000!");

      uint32_t current_block_num = get_head_block_num();
      const auto get_transactions_from_prior_block = [&]() -> vector<market_transaction>
      {
          auto itr = my->_market_transactions_db.lower_bound( current_block_num );
          if( current_block_num == get_head_block_num() )
              itr = my->_market_transactions_db.last();

          if (itr.valid()) --itr;
          if (itr.valid())
          {
              current_block_num = itr.key();
              return itr.value();
          }
          current_block_num = 1;
          return vector<market_transaction>();
      };

      FC_ASSERT(current_block_num > 0, "No blocks have been created yet!");
      auto orders = get_transactions_from_prior_block();

      std::function<bool(const market_transaction&)> order_is_uninteresting =
          [&quote,&base,&owner,this](const market_transaction& order) -> bool
      {
          //If it's in the market pair we're interested in, it might be interesting or uninteresting
          if( order.ask_index.order_price.base_asset_id == base
              && order.ask_index.order_price.quote_asset_id == quote ) {
            //If we're not filtering for a specific owner, it's interesting (not uninteresting)
            if (owner == address())
              return false;
            //If neither the bidder nor the asker is the owner I'm looking for, it's uninteresting
            return owner != order.bid_index.owner && owner != order.ask_index.owner;
          }
          //If it's not the market pair we're interested in, it's definitely uninteresting
          return true;
      };
      //Filter out orders not in our current market of interest
      orders.erase(std::remove_if(orders.begin(), orders.end(), order_is_uninteresting), orders.end());

      //While the next entire block of orders should be skipped...
      while( skip_count > 0 && --current_block_num > 0 && orders.size() <= skip_count ) {
        ilog("Skipping ${num} block ${block} orders", ("num", orders.size())("block", current_block_num));
        skip_count -= orders.size();
        orders = get_transactions_from_prior_block();
        orders.erase(std::remove_if(orders.begin(), orders.end(), order_is_uninteresting), orders.end());
      }

      if( current_block_num == 0 && orders.size() <= skip_count )
        // Skip count is greater or equal to the total number of relevant orders on the blockchain.
        return vector<order_history_record>();

      //If there are still some orders from the last block inspected to skip, remove them
      if( skip_count > 0 )
        orders.erase(orders.begin(), orders.begin() + skip_count);
      ilog("Building up order history, got ${num} so far...", ("num", orders.size()));

      std::vector<order_history_record> results;
      results.reserve(limit);
      fc::time_point_sec stamp = get_block_header(current_block_num).timestamp;
      for( const auto& rec : orders )
        results.push_back(order_history_record(rec, stamp));

      //While we still need more orders to reach our limit...
      while( --current_block_num >= 1 && orders.size() < limit )
      {
        auto more_orders = get_transactions_from_prior_block();
        more_orders.erase(std::remove_if(more_orders.begin(), more_orders.end(), order_is_uninteresting), more_orders.end());
        ilog("Found ${num} more orders in block ${block}...", ("num", more_orders.size())("block", current_block_num));
        stamp = get_block_header(current_block_num).timestamp;
        for( const auto& rec : more_orders )
          if( results.size() < limit )
            results.push_back(order_history_record(rec, stamp));
          else
            return results;
      }

      return results;
   }

    void chain_database::generate_issuance_map( const string& symbol, const fc::path& filename )const
    { try {
        map<string, share_type> issuance_map;
        auto uia = get_asset_record( symbol );
        FC_ASSERT( uia.valid() );
        FC_ASSERT( NOT uia->is_market_issued() );
        const auto scan_trx = [&]( const transaction_record trx_rec )
        {
            // Did we issue the asset in this transaction?
            bool issued = false;
            for( auto op : trx_rec.trx.operations )
            {
                if( op.type == issue_asset_op_type )
                {
                    auto issue = op.as<issue_asset_operation>();
                    if( issue.amount.asset_id != uia->id )
                        continue;
                    issued = true;
                    break;
                }
            }
            if( issued )
            {
                for( auto op : trx_rec.trx.operations )
                {
                    // make sure we didn't withdraw any of that op, that would only happen when someone is trying to be tricky
                    if( op.type == withdraw_op_type )
                    {
                        auto withdraw = op.as<withdraw_operation>();
                        auto obal = get_balance_record( withdraw.balance_id );
                        FC_ASSERT( obal->asset_id() != uia->id, "There was a withdraw for this UIA in an op that issued it!" );
                    }
                    if( op.type == deposit_op_type )
                    {
                        auto deposit = op.as<deposit_operation>();
                        if( deposit.condition.asset_id != uia->id )
                            continue;
                        FC_ASSERT( deposit.condition.type == withdraw_signature_type,
                           "I can't process deposits for any condition except withdraw_signature yet");
                        auto raw_addr = string( *deposit.condition.owner() );
                        if( issuance_map.find( raw_addr ) != issuance_map.end() )
                            issuance_map[ raw_addr ] += deposit.amount;
                        else
                            issuance_map[ raw_addr ] = deposit.amount;
                    }
                }
            }
        };
        scan_transactions( scan_trx );
        fc::json::save_to_file( issuance_map, filename );
    } FC_CAPTURE_AND_RETHROW( (symbol)(filename) ) }

   // NOTE: Only base asset 0 is snapshotted and addresses can have multiple entries
   void chain_database::generate_snapshot( const fc::path& filename )const
   { try {
       genesis_state snapshot = get_builtin_genesis_block_config();
       snapshot.timestamp = now();
       snapshot.initial_balances.clear();
       snapshot.sharedrop_balances.vesting_balances.clear();

       // Add standard withdraw_signature balances and unclaimed withdraw_vesting balances
       // NOTE: Unclaimed but mature vesting balances are not currently claimed
       for( auto iter = my->_balance_id_to_record.unordered_begin();
            iter != my->_balance_id_to_record.unordered_end(); ++iter )
       {
           const balance_record& record = iter->second;
           if( record.asset_id() != 0 ) continue;

           genesis_balance balance;
           if( record.snapshot_info.valid() )
           {
               balance.raw_address = record.snapshot_info->original_address;
           }
           else
           {
               const auto owner = record.owner();
               if( !owner.valid() ) continue;
               balance.raw_address = string( *owner );
           }
           balance.balance = record.balance;

           if( record.condition.type == withdraw_signature_type )
               snapshot.initial_balances.push_back( balance );
           else if( record.condition.type == withdraw_vesting_type )
               snapshot.sharedrop_balances.vesting_balances.push_back( balance );
       }

       // Add outstanding delegate pay balances
       for( auto iter = my->_account_id_to_record.unordered_begin();
            iter != my->_account_id_to_record.unordered_end(); ++iter )
       {
           const account_record& record = iter->second;
           if( !record.is_delegate() ) continue;
           if( record.is_retracted() ) continue;

           genesis_balance balance;
           balance.raw_address = string( record.active_address() );
           balance.balance = record.delegate_pay_balance();

           snapshot.initial_balances.push_back( balance );
       }

       // Add outstanding ask order balances
       for( auto iter = my->_ask_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& market_index = iter.key();
           if( market_index.order_price.base_asset_id != 0 ) continue;

           const order_record& record = iter.value();

           genesis_balance balance;
           balance.raw_address = string( market_index.owner );
           balance.balance = record.balance;

           snapshot.initial_balances.push_back( balance );
       }

       // Add outstanding short order balances
       for( auto iter = my->_short_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& market_index = iter.key();
           const order_record& record = iter.value();

           genesis_balance balance;
           balance.raw_address = string( market_index.owner );
           balance.balance = record.balance;

           snapshot.initial_balances.push_back( balance );
       }

       // Add outstanding collateral balances
       for( auto iter = my->_collateral_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& market_index = iter.key();
           const collateral_record& record = iter.value();

           genesis_balance balance;
           balance.raw_address = string( market_index.owner );
           balance.balance = record.collateral_balance;

           snapshot.initial_balances.push_back( balance );
       }

       fc::json::save_to_file( snapshot, filename );
   } FC_CAPTURE_AND_RETHROW( (filename) ) }

   void chain_database::graphene_snapshot( const string& filename, const set<string>& whitelist )const
   { try {
       snapshot_state snapshot;
       snapshot.head_block = get_head_block();
       snapshot.max_core_supply = calculate_max_core_supply( BTS_MAX_DELEGATE_PAY_PER_BLOCK );

       const auto accumulate_balance = [ & ]( const address& owner, const asset balance )
       {
           if( balance.amount == 0 )
               return;

           auto& snapshot_balance = snapshot.balances[ owner ];

           const auto asset_record = get_asset_record( balance.asset_id );
           FC_ASSERT( asset_record.valid() );
           snapshot_balance.assets[ asset_record->symbol ] += balance.amount;
       };

       const auto accumulate_vesting_balance = [ & ]( const address& owner, const share_type balance, const withdraw_vesting& contract )
       {
           if( balance == 0 )
               return;

           auto& snapshot_balance = snapshot.balances[ owner ];
           if( !snapshot_balance.vesting.valid() )
           {
               snapshot_balance.vesting = snapshot_vesting_balance();
               snapshot_balance.vesting->start_time = contract.start_time;
               snapshot_balance.vesting->duration_seconds = contract.duration;
           }

           auto& snapshot_vesting_balance = *snapshot_balance.vesting;
           FC_ASSERT( snapshot_vesting_balance.start_time == contract.start_time );
           FC_ASSERT( snapshot_vesting_balance.duration_seconds == contract.duration );

           snapshot_vesting_balance.original_balance += contract.original_balance;
           snapshot_vesting_balance.current_balance += balance;
       };

       const auto scan_balance = [ & ]( const balance_record& record )
       {
           if( record.balance == 0 )
               return;

           const auto owner = record.owner();
           if( !owner.valid() )
           {
               // TODO: Dump pretty-print of record for escrow/multisig paybacks
               return;
           }

           const asset balance = record.get_spendable_balance( now() );
           share_type unvested_balance = 0;
           if( balance.amount != record.balance )
           {
               FC_ASSERT( record.condition.type == withdraw_vesting_type );
               FC_ASSERT( record.balance > balance.amount );
               FC_ASSERT( balance.asset_id == asset_id_type( 0 ) );
               unvested_balance += record.balance - balance.amount;

               accumulate_vesting_balance( *owner, unvested_balance, record.condition.as<withdraw_vesting>() );
           }
           else
           {
               FC_ASSERT( record.condition.type == withdraw_signature_type );
           }

           accumulate_balance( *owner, balance );
       };
       scan_balances( scan_balance );

       const auto scan_account = [ & ]( const account_record& record )
       {
           share_type pay_balance = 0;
           if( record.is_delegate() )
               pay_balance += record.delegate_info->pay_balance;

           accumulate_balance( record.owner_address(), asset( pay_balance ) );

           if( record.is_retracted() )
               return;

           if( record.registration_date <= get_genesis_timestamp() && record.name.find( "init" ) == 0 )
               return;

           auto& snapshot_account = snapshot.accounts[ record.name ];
           snapshot_account.id = uint32_t( record.id );
           snapshot_account.timestamp = record.registration_date;
           snapshot_account.owner_key = record.owner_key;
           snapshot_account.active_key = record.active_key();

           if( record.is_delegate() )
           {
               // Witness
               if( record.registration_date > get_genesis_timestamp() || record.delegate_info->blocks_produced > 0 )
                   snapshot_account.signing_key = record.signing_key();

               // Worker
               if( record.delegate_info->pay_rate > 3 )
               {
                   snapshot_account.daily_pay = BTS_BLOCKCHAIN_BLOCKS_PER_DAY * BTS_MAX_DELEGATE_PAY_PER_BLOCK * record.delegate_info->pay_rate
                                                / BTS_BLOCKCHAIN_NUM_DELEGATES / 100;
               }
           }
       };
       scan_unordered_accounts( scan_account );

       for( auto iter = my->_ask_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& index = iter.key();
           const order_record& ask = iter.value();
           accumulate_balance( index.owner, asset( ask.balance, index.order_price.base_asset_id ) );

           ++snapshot.summary.num_canceled_asks;
       }

       for( auto iter = my->_bid_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& index = iter.key();
           const order_record& bid = iter.value();
           accumulate_balance( index.owner, asset( bid.balance, index.order_price.quote_asset_id ) );

           ++snapshot.summary.num_canceled_bids;
       }

       for( auto iter = my->_short_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& index = iter.key();
           const order_record& sh = iter.value();
           accumulate_balance( index.owner, asset( sh.balance ) );

           ++snapshot.summary.num_canceled_shorts;
       }

       const auto scan_asset = [ & ]( const asset_record& record )
       {
           // CORE fees go back into reserve fund
           if( record.id == 0 )
               return;

           auto& snapshot_asset = snapshot.assets[ record.symbol ];

           snapshot_asset.id = uint32_t( record.id );

           if( record.is_market_issued() )
           {
               snapshot_asset.owner = "witness-account";
           }
           else if( record.is_user_issued() )
           {
               const auto account_record = get_account_record( record.issuer_id );
               FC_ASSERT( account_record.valid() );
               snapshot_asset.owner = account_record->name;
           }

           snapshot_asset.description = record.description;
           snapshot_asset.precision = record.precision;

           snapshot_asset.max_supply = record.max_supply;
           snapshot_asset.collected_fees = std::max( record.collected_fees, share_type( 0 ) );
       };
       scan_unordered_assets( scan_asset );

       for( auto iter = my->_collateral_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& index = iter.key();
           const collateral_record& collateral = iter.value();

           const auto asset_record = get_asset_record( index.order_price.quote_asset_id );
           FC_ASSERT( asset_record.valid() );

           auto& snapshot_asset = snapshot.assets[ asset_record->symbol ];
           snapshot_asset.debts[ index.owner ].collateral += collateral.collateral_balance;
           snapshot_asset.debts[ index.owner ].debt += collateral.payoff_balance;
       }

       // Hack to rectify supply/debt differences for GOLD
       for( auto& item : snapshot.assets )
       {
           const auto& symbol = item.first;
           auto& asset = item.second;
           if( asset.owner != "witness-account" )
               continue;

           FC_ASSERT( asset.collected_fees >= 0 );
           share_type total_supply = asset.collected_fees;
           share_type total_debt = 0;

           for( const auto& item : snapshot.balances )
           {
               const auto& balance = item.second;
               if( balance.assets.count( symbol ) > 0 )
                   total_supply += balance.assets.at( symbol );
           }

           for( const auto& item : asset.debts )
           {
               total_debt += item.second.debt;
           }

           if( total_supply != total_debt )
           {
               FC_ASSERT( total_supply > total_debt );

               vector<pair<address, snapshot_debt>> debt_list;
               for( const auto& item: asset.debts )
                   debt_list.push_back( std::make_pair( item.first, item.second ) );

               const auto sorter = []( const pair<address, snapshot_debt>& a, const pair<address, snapshot_debt>& b ) -> bool
               {
                   return a.second.debt < b.second.debt;
               };
               std::sort( debt_list.begin(), debt_list.end(), sorter );
               std::reverse( debt_list.begin(), debt_list.end() );

               const auto multiplier = double( total_supply - total_debt ) / total_supply + 1;

               share_type new_total_debt = 0;
               for( auto& item : debt_list )
               {
                   const auto new_debt = item.second.debt * multiplier;
                   item.second.debt = new_debt;
                   new_total_debt += new_debt;
               }

               while( !debt_list.empty() )
               {
                   if( new_total_debt >= total_supply )
                       break;

                   for( auto& item : debt_list )
                   {
                       if( new_total_debt >= total_supply )
                           break;

                       ++item.second.debt;
                       ++new_total_debt;
                   }
               }

               for( auto& item : debt_list )
                   asset.debts[ item.first ] = item.second;

               total_supply = asset.collected_fees;
               total_debt = 0;

               for( const auto& item : snapshot.balances )
               {
                   const auto& balance = item.second;
                   if( balance.assets.count( symbol ) > 0 )
                       total_supply += balance.assets.at( symbol );
               }

               for( const auto& item : asset.debts )
               {
                   total_debt += item.second.debt;
               }

               FC_ASSERT( total_supply == total_debt );
           }
       }

       // Tally summary statistics

       snapshot.summary.num_balance_owners = snapshot.balances.size();
       for( const auto& item : snapshot.balances )
       {
           const auto& balance = item.second;

           snapshot.summary.num_asset_balances += balance.assets.size();

           if( balance.vesting.valid() )
               ++snapshot.summary.num_vesting_balances;
       }

       snapshot.summary.num_account_names = snapshot.accounts.size();
       for( const auto& item : snapshot.accounts )
       {
           const auto& account = item.second;

           if( account.signing_key.valid() )
               ++snapshot.summary.num_witnesses;

           if( account.daily_pay.valid() )
               ++snapshot.summary.num_workers;
       }

       for( const auto& item : snapshot.assets )
       {
           const auto& asset = item.second;

           if( asset.owner == "witness-account" )
               ++snapshot.summary.num_mias;
           else
               ++snapshot.summary.num_uias;

           snapshot.summary.num_collateral_positions += asset.debts.size();
       }

       fc::json::save_to_file( snapshot, fc::path( "intermediate-" + filename ) );

       // Transform to Graphene import format
       genesis_state_type genesis;

       genesis.initial_timestamp = snapshot.head_block.timestamp;
       genesis.max_core_supply = snapshot.max_core_supply;

       const auto is_cheap_name = []( const string& n ) -> bool
       {
           bool v = false;
           for( auto c : n )
           {
              if( c >= '0' && c <= '9' ) return true;
              if( c == '.' || c == '-' || c == '/' ) return true;
              switch( c )
              {
                 case 'a':
                 case 'e':
                 case 'i':
                 case 'o':
                 case 'u':
                 case 'y':
                    v = true;
              }
           }
           if( !v )
              return true;
           return false;
       };

       static const auto june8 = time_point_sec( 1433736000 );
       static const auto june18 = time_point_sec( 1434600000 );

       // Accounts, witnesses, and workers
       // TODO: Don't forget name modifications and blacklists
       for( const auto& item : snapshot.accounts )
       {
           const string& name = item.first;
           const snapshot_account& keys = item.second;

           bool is_prefixed = false;
           if( keys.timestamp >= june8 )
           {
               if( keys.timestamp >= june18 )
               {
                   if( whitelist.count( name ) == 0 )
                       is_prefixed = true;
               }
               else if( !is_cheap_name( name ) )
               {
                   is_prefixed = true;
               }
           }

           genesis_state_type::initial_account_type account;
           account.name = name;
           account.owner_key = keys.owner_key;
           account.active_key = keys.active_key;
           account.is_lifetime_member = keys.signing_key.valid() || keys.daily_pay.valid();
           account.is_prefixed = is_prefixed;
           account.id = keys.id;

           genesis.initial_accounts.push_back( std::move( account ) );

           // Witness
           if( keys.signing_key.valid() )
           {
               genesis_state_type::initial_witness_type witness;
               witness.owner_name = name;
               witness.block_signing_key = *keys.signing_key;

               genesis.initial_witness_candidates.push_back( std::move( witness ) );
           }

           // Worker
           if( keys.daily_pay.valid() )
           {
               genesis_state_type::initial_worker_type worker;
               worker.owner_name = name;
               worker.daily_pay = *keys.daily_pay;

               genesis.initial_worker_candidates.push_back( std::move( worker ) );
           }
       }

       // Balances and vesting balances
       for( const auto& item : snapshot.balances )
       {
           const address& owner = item.first;
           const snapshot_balance& balances = item.second;

           // Balances
           for( const auto& asset_item : balances.assets )
           {
               genesis_state_type::initial_balance_type balance{ owner, asset_item.first, asset_item.second };
               genesis.initial_balances.push_back( std::move( balance ) );
           }

           // Vesting balances
           if( balances.vesting.valid() )
           {
               const snapshot_vesting_balance& vesting = *balances.vesting;

               genesis_state_type::initial_vesting_balance_type vesting_balance;
               vesting_balance.owner = owner;
               vesting_balance.asset_symbol = BTS_BLOCKCHAIN_SYMBOL;
               vesting_balance.amount = vesting.current_balance;
               vesting_balance.begin_timestamp = vesting.start_time;
               vesting_balance.vesting_duration_seconds = vesting.duration_seconds;
               vesting_balance.begin_balance = vesting.original_balance;

               genesis.initial_vesting_balances.push_back( std::move( vesting_balance ) );
           }
       }

       const auto convert_precision = [ & ]( uint64_t original_precision ) -> uint8_t
       {
           uint8_t converted_precision = 0;
           for( ; original_precision > 1; original_precision /= 10 )
               ++converted_precision;

           return converted_precision;
       };

       for( const auto& item : snapshot.assets )
       {
           const string& symbol = item.first;
           const auto& snapshot_asset = item.second;

           genesis_state_type::initial_asset_type asset;
           asset.symbol = symbol;
           asset.issuer_name = snapshot_asset.owner;

           asset.description = snapshot_asset.description;
           asset.precision = convert_precision( snapshot_asset.precision );

           asset.max_supply = snapshot_asset.max_supply;
           asset.accumulated_fees = snapshot_asset.collected_fees;

           asset.is_bitasset = snapshot_asset.owner == string( "witness-account" );
           asset.id = snapshot_asset.id;

           for( const auto& debt_item : snapshot_asset.debts )
           {
               genesis_state_type::initial_asset_type::initial_collateral_position position;
               position.owner = debt_item.first;
               position.collateral = debt_item.second.collateral;
               position.debt = debt_item.second.debt;

               asset.collateral_records.push_back( std::move( position ) );
           }

           genesis.initial_assets.push_back( std::move( asset ) );
       }

       fc::json::save_to_file( genesis, fc::path( filename ) );
   } FC_CAPTURE_AND_RETHROW( (filename) ) }

   unordered_map<asset_id_type, share_type> chain_database::calculate_supplies()const
   { try {
       unordered_map<asset_id_type, share_type> totals;

       for( auto iter = my->_account_id_to_record.unordered_begin(); iter != my->_account_id_to_record.unordered_end(); ++iter )
       {
           const account_record& account = iter->second;
           if( account.delegate_info.valid() )
               totals[ 0 ] += account.delegate_info->pay_balance;
       }

       for( auto iter = my->_asset_id_to_record.unordered_begin(); iter != my->_asset_id_to_record.unordered_end(); ++iter )
       {
           const asset_record& asset = iter->second;
           totals[ asset.id ] += asset.collected_fees;
       }

       for( auto iter = my->_balance_id_to_record.unordered_begin(); iter != my->_balance_id_to_record.unordered_end(); ++iter )
       {
           const balance_record& balance = iter->second;
           totals[ balance.asset_id() ] += balance.balance;
       }

       for( auto iter = my->_ask_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& index = iter.key();
           const order_record& ask = iter.value();
           totals[ index.order_price.base_asset_id ] += ask.balance;
       }

       for( auto iter = my->_bid_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& index = iter.key();
           const order_record& bid = iter.value();
           totals[ index.order_price.quote_asset_id ] += bid.balance;
       }

       for( auto iter = my->_short_db.begin(); iter.valid(); ++iter )
       {
           const order_record& sh = iter.value();
           totals[ 0 ] += sh.balance;
       }

       for( auto iter = my->_collateral_db.begin(); iter.valid(); ++iter )
       {
           const collateral_record& collateral = iter.value();
           totals[ 0 ] += collateral.collateral_balance;
       }

       return totals;
   } FC_CAPTURE_AND_RETHROW() }

   unordered_map<asset_id_type, share_type> chain_database::calculate_debts( bool include_interest )const
   { try {
       unordered_map<asset_id_type, share_type> totals;

       for( auto iter = my->_collateral_db.begin(); iter.valid(); ++iter )
       {
           const market_index_key& index = iter.key();
           const collateral_record& collateral = iter.value();
           const asset principle = asset( collateral.payoff_balance, index.order_price.quote_asset_id );
           totals[ principle.asset_id ] += principle.amount;

           if( include_interest )
           {
               const time_point_sec position_start_time = collateral.expiration - BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC;
               const uint32_t position_age = (now() - position_start_time).to_seconds();
               const asset interest = detail::market_engine::get_interest_owed( principle,
                                                                                collateral.interest_rate,
                                                                                position_age );
               totals[ interest.asset_id ] += interest.amount;
           }
       }

       return totals;
   } FC_CAPTURE_AND_RETHROW( (include_interest) ) }

   share_type chain_database::calculate_max_core_supply( share_type pay_per_block )const
   { try {
       share_type max_supply = calculate_supplies().at( 0 );

       static const time_point_sec start_timestamp = get_genesis_timestamp();
       static const uint32_t seconds_per_period = fc::days( 4 * 365 ).to_seconds(); // Ignore leap years, leap seconds, etc.

       time_point_sec interval_start = this->now();
       time_point_sec interval_end = start_timestamp + seconds_per_period;

       for( ; pay_per_block > 0; pay_per_block /= 2 )
       {
           const uint32_t interval_duration = (interval_end - interval_start).to_seconds();

           max_supply += interval_duration * pay_per_block / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;

           interval_start = interval_end;
           interval_end += seconds_per_period;
       }

       return max_supply;
   } FC_CAPTURE_AND_RETHROW( (pay_per_block) ) }

   asset chain_database::unclaimed_genesis()
   { try {
        asset unclaimed_total;
        const auto genesis_date = get_genesis_timestamp();
        const auto scan_balance = [ &unclaimed_total, &genesis_date ]( const balance_record& record )
        {
            if( record.snapshot_info.valid() )
            {
                if( record.last_update <= genesis_date )
                    unclaimed_total.amount += record.balance;
            }
        };
        scan_balances( scan_balance );
        return unclaimed_total;
   } FC_CAPTURE_AND_RETHROW() }

   oprice chain_database::get_active_feed_price( const asset_id_type quote_id )const
   { try {
       const auto outer_iter = my->_nested_feed_map.find( quote_id );
       if( outer_iter == my->_nested_feed_map.end() )
           return oprice();

       // TODO: Caller passes in delegate list
       const vector<account_id_type>& delegate_ids = get_active_delegates();

       vector<price> prices;
       prices.reserve( delegate_ids.size() );

       const time_point_sec now = this->now();
       static const auto limit = fc::days( 1 );

       const unordered_map<account_id_type, feed_record>& delegate_feeds = outer_iter->second;
       for( const account_id_type delegate_id : delegate_ids )
       {
           const auto iter = delegate_feeds.find( delegate_id );
           if( iter == delegate_feeds.end() ) continue;

           const feed_record& record = iter->second;
           if( time_point( now ) - time_point( record.last_update ) >= limit ) continue;

           if( record.value.quote_asset_id != quote_id ) continue;
           if( record.value.base_asset_id != 0 ) continue;

           prices.push_back( record.value );
       }

       if( prices.size() >= BTS_BLOCKCHAIN_MIN_FEEDS )
       {
           const auto midpoint = prices.size() / 2;
           std::nth_element( prices.begin(), prices.begin() + midpoint, prices.end() );
           return prices.at( midpoint );
       }

       return oprice();
   } FC_CAPTURE_AND_RETHROW( (quote_id) ) }

   vector<feed_record> chain_database::get_feeds_for_asset( const asset_id_type quote_id, const asset_id_type base_id )const
   {  try {
      vector<feed_record> feeds;
      auto feed_itr = my->_feed_index_to_record.lower_bound(feed_index{quote_id});
      while( feed_itr.valid() && feed_itr.key().quote_id == quote_id )
      {
        const auto& val = feed_itr.value();
        if( val.value.base_asset_id == base_id )
           feeds.push_back(val);
        ++feed_itr;
      }
      return feeds;
   } FC_CAPTURE_AND_RETHROW( (quote_id)(base_id) ) }

   vector<feed_record> chain_database::get_feeds_from_delegate( const account_id_type delegate_id )const
   {  try {
      vector<feed_record> records;
      for( auto iter = my->_feed_index_to_record.begin(); iter.valid(); ++iter )
      {
          const feed_record& record = iter.value();
          if( record.index.delegate_id == delegate_id )
              records.push_back( record );
      }
      return records;
   } FC_CAPTURE_AND_RETHROW( (delegate_id) ) }

   vector<burn_record> chain_database::fetch_burn_records( const string& account_name )const
   { try {
      vector<burn_record> results;
      const auto opt_account_record = get_account_record( account_name );
      FC_ASSERT( opt_account_record.valid() );

      auto itr = my->_burn_index_to_record.lower_bound( {opt_account_record->id} );
      while( itr.valid() && itr.key().account_id == opt_account_record->id )
      {
         results.push_back( itr.value() );
         ++itr;
      }

      itr = my->_burn_index_to_record.lower_bound( {-opt_account_record->id} );
      while( itr.valid() && itr.key().account_id == -opt_account_record->id )
      {
         results.push_back( itr.value() );
         ++itr;
      }

      return results;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   vector<transaction_record> chain_database::fetch_address_transactions( const address& addr )
   { try {
      vector<transaction_record> results;

      const auto transaction_ids = my->_address_to_transaction_ids.fetch_optional( addr );
      if( transaction_ids.valid() )
      {
          for( const transaction_id_type& transaction_id : *transaction_ids )
          {
              otransaction_record record = get_transaction( transaction_id );
              if( record.valid() ) results.push_back( std::move( *record ) );
          }
      }

      return results;
   } FC_CAPTURE_AND_RETHROW( (addr) ) }

   oproperty_record chain_database::property_lookup_by_id( const property_id_type id )const
   {
       const auto iter = my->_property_id_to_record.unordered_find( static_cast<uint8_t>( id ) );
       if( iter != my->_property_id_to_record.unordered_end() ) return iter->second;
       return oproperty_record();
   }

   void chain_database::property_insert_into_id_map( const property_id_type id, const property_record& record )
   {
       my->_property_id_to_record.store( static_cast<uint8_t>( id ), record );
   }

   void chain_database::property_erase_from_id_map( const property_id_type id )
   {
       my->_property_id_to_record.remove( static_cast<uint8_t>( id ) );
   }

   oaccount_record chain_database::account_lookup_by_id( const account_id_type id )const
   {
       const auto iter = my->_account_id_to_record.unordered_find( id );
       if( iter != my->_account_id_to_record.unordered_end() ) return iter->second;
       return oaccount_record();
   }

   oaccount_record chain_database::account_lookup_by_name( const string& name )const
   {
       const auto iter = my->_account_name_to_id.unordered_find( name );
       if( iter != my->_account_name_to_id.unordered_end() ) return account_lookup_by_id( iter->second );
       return oaccount_record();
   }

   oaccount_record chain_database::account_lookup_by_address( const address& addr )const
   {
       const auto iter = my->_account_address_to_id.unordered_find( addr );
       if( iter != my->_account_address_to_id.unordered_end() ) return account_lookup_by_id( iter->second );
       return oaccount_record();
   }

   void chain_database::account_insert_into_id_map( const account_id_type id, const account_record& record )
   {
       my->_account_id_to_record.store( id, record );
   }

   void chain_database::account_insert_into_name_map( const string& name, const account_id_type id )
   {
       my->_account_name_to_id.store( name, id );
   }

   void chain_database::account_insert_into_address_map( const address& addr, const account_id_type id )
   {
       my->_account_address_to_id.store( addr, id );
   }

   void chain_database::account_insert_into_vote_set( const vote_del& vote )
   {
       my->_delegate_votes.insert( vote );
   }

   void chain_database::account_erase_from_id_map( const account_id_type id )
   {
       my->_account_id_to_record.remove( id );
   }

   void chain_database::account_erase_from_name_map( const string& name )
   {
       my->_account_name_to_id.remove( name );
   }

   void chain_database::account_erase_from_address_map( const address& addr )
   {
       my->_account_address_to_id.remove( addr );
   }

   void chain_database::account_erase_from_vote_set( const vote_del& vote )
   {
       my->_delegate_votes.erase( vote );
   }

   oasset_record chain_database::asset_lookup_by_id( const asset_id_type id )const
   {
       const auto iter = my->_asset_id_to_record.unordered_find( id );
       if( iter != my->_asset_id_to_record.unordered_end() ) return iter->second;
       return oasset_record();
   }

   oasset_record chain_database::asset_lookup_by_symbol( const string& symbol )const
   {
       const auto iter = my->_asset_symbol_to_id.unordered_find( symbol );
       if( iter != my->_asset_symbol_to_id.unordered_end() ) return asset_lookup_by_id( iter->second );
       return oasset_record();
   }

   void chain_database::asset_insert_into_id_map( const asset_id_type id, const asset_record& record )
   {
       my->_asset_id_to_record.store( id, record );
   }

   void chain_database::asset_insert_into_symbol_map( const string& symbol, const asset_id_type id )
   {
       my->_asset_symbol_to_id.store( symbol, id );
   }

   void chain_database::asset_erase_from_id_map( const asset_id_type id )
   {
       my->_asset_id_to_record.remove( id );
   }

   void chain_database::asset_erase_from_symbol_map( const string& symbol )
   {
       my->_asset_symbol_to_id.remove( symbol );
   }

   oslate_record chain_database::slate_lookup_by_id( const slate_id_type id )const
   {
       const auto iter = my->_slate_id_to_record.unordered_find( id );
       if( iter != my->_slate_id_to_record.unordered_end() ) return iter->second;
       return oslate_record();
   }

   void chain_database::slate_insert_into_id_map( const slate_id_type id, const slate_record& record )
   {
       my->_slate_id_to_record.store( id, record );
   }

   void chain_database::slate_erase_from_id_map( const slate_id_type id )
   {
       my->_slate_id_to_record.remove( id );
   }

   obalance_record chain_database::balance_lookup_by_id( const balance_id_type& id )const
   {
       const auto iter = my->_balance_id_to_record.unordered_find( id );
       if( iter != my->_balance_id_to_record.unordered_end() ) return iter->second;
       return obalance_record();
   }

   void chain_database::balance_insert_into_id_map( const balance_id_type& id, const balance_record& record )
   {
       my->_balance_id_to_record.store( id, record );
   }

   void chain_database::balance_erase_from_id_map( const balance_id_type& id )
   {
       my->_balance_id_to_record.remove( id );
   }

   otransaction_record chain_database::transaction_lookup_by_id( const transaction_id_type& id )const
   {
       return my->_transaction_id_to_record.fetch_optional( id );
   }

   void chain_database::transaction_insert_into_id_map( const transaction_id_type& id, const transaction_record& record )
   {
       my->_transaction_id_to_record.store( id, record );

       if( get_statistics_enabled() )
       {
           const auto scan_address = [ & ]( const address& addr )
           {
               auto ids = my->_address_to_transaction_ids.fetch_optional( addr );
               if( !ids.valid() ) ids = unordered_set<transaction_id_type>();
               ids->insert( id );
               my->_address_to_transaction_ids.store( addr, *ids );
           };
           record.scan_addresses( *this, scan_address );

           for( const operation& op : record.trx.operations )
               store_recent_operation( op );
       }
   }

   void chain_database::transaction_insert_into_unique_set( const transaction& trx )
   {
       my->_unique_transactions.emplace_hint( my->_unique_transactions.cend(), trx, get_chain_id() );
   }

   void chain_database::transaction_erase_from_id_map( const transaction_id_type& id )
   {
       if( get_statistics_enabled() )
       {
           const otransaction_record record = transaction_lookup_by_id( id );
           if( record.valid() )
           {
               const auto scan_address = [ & ]( const address& addr )
               {
                   auto ids = my->_address_to_transaction_ids.fetch_optional( addr );
                   if( !ids.valid() ) return;
                   ids->erase( id );
                   if( !ids->empty() ) my->_address_to_transaction_ids.store( addr, *ids );
                   else my->_address_to_transaction_ids.remove( addr );
               };
               record->scan_addresses( *this, scan_address );
           }
       }

       my->_transaction_id_to_record.remove( id );
   }

   void chain_database::transaction_erase_from_unique_set( const transaction& trx )
   {
       my->_unique_transactions.erase( unique_transaction_key( trx, get_chain_id() ) );
   }

   oburn_record chain_database::burn_lookup_by_index( const burn_index& index )const
   {
       return my->_burn_index_to_record.fetch_optional( index );
   }

   void chain_database::burn_insert_into_index_map( const burn_index& index, const burn_record& record )
   {
       my->_burn_index_to_record.store( index, record );
   }

   void chain_database::burn_erase_from_index_map( const burn_index& index )
   {
       my->_burn_index_to_record.remove( index );
   }

   ostatus_record chain_database::status_lookup_by_index( const status_index index )const
   {
       return my->_status_index_to_record.fetch_optional( index );
   }

   void chain_database::status_insert_into_index_map( const status_index index, const status_record& record )
   {
       my->_status_index_to_record.store( index, record );
   }

   void chain_database::status_erase_from_index_map( const status_index index )
   {
       my->_status_index_to_record.remove( index );
   }

   ofeed_record chain_database::feed_lookup_by_index( const feed_index index )const
   {
       const auto outer_iter = my->_nested_feed_map.find( index.quote_id );
       if( outer_iter != my->_nested_feed_map.end() )
       {
           const auto inner_iter = outer_iter->second.find( index.delegate_id );
           if( inner_iter != outer_iter->second.end() )
               return inner_iter->second;

       }
       return ofeed_record();
   }

   void chain_database::feed_insert_into_index_map( const feed_index index, const feed_record& record )
   {
       my->_feed_index_to_record.store( index, record );
       my->_nested_feed_map[ index.quote_id ][ index.delegate_id ] = record;
   }

   void chain_database::feed_erase_from_index_map( const feed_index index )
   {
       my->_feed_index_to_record.remove( index );
       const auto outer_iter = my->_nested_feed_map.find( index.quote_id );
       if( outer_iter != my->_nested_feed_map.end() )
       {
           const auto inner_iter = outer_iter->second.find( index.delegate_id );
           if( inner_iter != outer_iter->second.end() )
               outer_iter->second.erase( index.delegate_id );
       }
   }

   oslot_record chain_database::slot_lookup_by_index( const slot_index index )const
   {
       return my->_slot_index_to_record.fetch_optional( index );
   }

   oslot_record chain_database::slot_lookup_by_timestamp( const time_point_sec timestamp )const
   {
       const optional<account_id_type> delegate_id = my->_slot_timestamp_to_delegate.fetch_optional( timestamp );
       if( !delegate_id.valid() ) return oslot_record();
       return my->_slot_index_to_record.fetch_optional( slot_index( *delegate_id, timestamp ) );
   }

   void chain_database::slot_insert_into_index_map( const slot_index index, const slot_record& record )
   {
       my->_slot_index_to_record.store( index, record );
   }

   void chain_database::slot_insert_into_timestamp_map( const time_point_sec timestamp, const account_id_type delegate_id )
   {
       my->_slot_timestamp_to_delegate.store( timestamp, delegate_id );
   }

   void chain_database::slot_erase_from_index_map( const slot_index index )
   {
       my->_slot_index_to_record.remove( index );
   }

   void chain_database::slot_erase_from_timestamp_map( const time_point_sec timestamp )
   {
       my->_slot_timestamp_to_delegate.remove( timestamp );
   }

   fc::variants chain_database::debug_get_matching_errors() const
   {
       return my->_debug_matching_error_log;
   }

   void chain_database::debug_trap_on_block( uint32_t blocknum )
   {
       my->_debug_trap_blocks.insert( blocknum );
       return;
   }

} } // bts::blockchain
