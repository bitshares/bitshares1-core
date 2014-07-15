//#define DEFAULT_LOGGER "blockchain"

#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/genesis_config.hpp>
#include <bts/blockchain/genesis_json.hpp>
#include <bts/blockchain/operation_factory.hpp>
#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/db/level_map.hpp>

#include <fc/thread/mutex.hpp>
#include <fc/thread/unique_lock.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw_variant.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

using namespace bts::blockchain;

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

FC_REFLECT_TYPENAME( std::vector<bts::blockchain::block_id_type> )


namespace bts { namespace blockchain {

   // register exceptions here so it doesn't get optimized out by the linker
   FC_REGISTER_EXCEPTIONS( (blockchain_exception)
                           (invalid_pts_address)
                          (addition_overflow)
                          (addition_underthrow)
                          (asset_type_mismatch)
                          (unsupported_chain_operation) )


   namespace detail
   {

      class chain_database_impl
      {
         public:
            chain_database_impl():self(nullptr){}

            class market_engine
            {
               public:
                  market_engine( pending_chain_state_ptr ps, chain_database_impl& cdi )
                  :_pending_state(ps),_db_impl(cdi){}

                  void execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec& timestamp )
                  { try {
                       _quote_id = quote_id;
                       _base_id = base_id;
                       // the order book is soreted from low to high price, so to get the last item (highest bid), we need to go to the first item in the
                       // next market class and then back up one
                       auto next_pair  = base_id+1 == quote_id ? price( 0, quote_id+1, 0) : price( 0, quote_id, base_id+1 );
                       _bid_itr        = _db_impl._bid_db.lower_bound( market_index_key( next_pair ) );
                       _ask_itr        = _db_impl._ask_db.lower_bound( market_index_key( price( 0, quote_id, base_id) ) );
                       _short_itr      = _db_impl._short_db.lower_bound( market_index_key( next_pair ) );
                       _collateral_itr      = _db_impl._collateral_db.lower_bound( market_index_key( next_pair ) );

                       if( !_ask_itr.valid() )
                       {
                          wlog( "ask iter invalid..." );
                          _ask_itr = _db_impl._ask_db.begin();
                       }

                       if( _short_itr.valid() ) --_short_itr;
                       else _short_itr = _db_impl._short_db.last();

                       if( _bid_itr.valid() )   --_bid_itr;
                       else _bid_itr = _db_impl._bid_db.last();

                       if( _collateral_itr.valid() )   --_collateral_itr;
                       else _collateral_itr = _db_impl._collateral_db.last();


                       asset xts_fees_collected(0,base_id);
                       asset trading_volume(0, base_id);

                       price last_valid_bid;
                       last_valid_bid.ratio = price::infinite();
                       price last_valid_ask;
                       last_valid_ask.ratio = price::infinite();

                       get_next_ask();
                       while( get_next_bid() && get_next_ask() )
                       {
                          last_valid_bid = _current_bid->get_price();
                          last_valid_ask = _current_ask->get_price();

                          idump( (_current_bid)(_current_ask) );
                          price ask_price = _current_ask->get_price();
                          // this works for bids, asks, and shorts.... but in the case of a cover
                          // the current ask can go lower than the call price in order to match 
                          // the bid.... 
                          if( _current_ask->type == cover_order )
                          {
                             ask_price = std::min( _current_bid->get_price(), _current_ask->get_highest_cover_price() );
                          }

                          if( _current_bid->get_price() < ask_price )
                             break;
                          auto quote_quantity = std::min( _current_bid->get_quote_quantity(), 
                                                          _current_ask->get_quote_quantity() );

                          auto usd_paid_by_bid     = quote_quantity;
                          auto usd_received_by_ask = quote_quantity;
                          auto xts_paid_by_ask     = quote_quantity * ask_price;
                          auto xts_received_by_bid = quote_quantity * _current_bid->get_price();

                          idump( (xts_fees_collected)(xts_paid_by_ask)(xts_received_by_bid)(quote_quantity) );
                          xts_fees_collected += xts_paid_by_ask - xts_received_by_bid;

                          market_transaction mtrx;
                          mtrx.bid_owner       = _current_bid->get_owner();
                          mtrx.bid_price       = _current_bid->get_price();
                          mtrx.bid_paid        = usd_paid_by_bid;
                          mtrx.bid_received    = xts_received_by_bid;
                          mtrx.ask_owner       = _current_ask->get_owner();
                          mtrx.ask_price       = ask_price;
                          mtrx.ask_paid        = xts_paid_by_ask;
                          mtrx.ask_received    = usd_received_by_ask;
                          mtrx.fees_collected  = xts_paid_by_ask - xts_received_by_bid;

                          _market_transactions.push_back(mtrx);
                          trading_volume += mtrx.bid_received;

                          if( _current_ask->type == ask_order )
                          {
                             /* rounding errors on price cause this not to go to 0 in some cases */
                             if( quote_quantity == _current_ask->get_quote_quantity() )
                                _current_ask->state.balance = 0; 
                             else
                                _current_ask->state.balance -= xts_paid_by_ask.amount;

                             auto ask_balance_address = withdraw_condition( withdraw_with_signature(_current_ask->get_owner()), quote_id ).get_address();
                             auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
                             if( !ask_payout )
                                ask_payout = balance_record( _current_ask->get_owner(), asset(0,quote_id), 0 );
                             ask_payout->balance += usd_received_by_ask.amount;

                             _pending_state->store_balance_record( *ask_payout );
                             _pending_state->store_ask_record( _current_ask->market_index, _current_ask->state );
                          }
                          else if( _current_ask->type == cover_order )
                          {
                             elog( "MATCHING COVER ORDER recv_usd: ${usd}  paid_collat: ${c}",
                                   ("usd",usd_received_by_ask)("c",xts_paid_by_ask) );
                             wlog( "current ask: ${c}", ("c",_current_ask) );
                             // we are in the margin call range... 
                             _current_ask->state.balance  -= usd_received_by_ask.amount;
                             *(_current_ask->collateral)  -= xts_paid_by_ask.amount;

                             if( _current_ask->state.balance == 0 ) // no more USD left
                             { // send collateral home to mommy & daddy
                                   wlog( "            collateral balance is now 0!" ); 
                                   auto ask_balance_address = withdraw_condition( 
                                                                     withdraw_with_signature(_current_ask->get_owner()), 
                                                                     base_id ).get_address();

                                   auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
                                   if( !ask_payout )
                                      ask_payout = balance_record( _current_ask->get_owner(), asset(0,base_id), 0 );
                                   ask_payout->balance += (*_current_ask->collateral);

                                   _pending_state->store_balance_record( *ask_payout );
                                   _current_ask->collateral = 0;

                             }
                             wlog( "storing collateral ${c}", ("c",_current_ask) );
                             _pending_state->store_collateral_record( _current_ask->market_index, 
                                                                      collateral_record( *_current_ask->collateral, 
                                                                                         _current_ask->state.balance ) );
                          }


                          if( _current_bid->type == bid_order )
                          {
                             _current_bid->state.balance -= usd_paid_by_bid.amount;

                             auto bid_payout = _pending_state->get_balance_record( 
                                                       withdraw_condition( withdraw_with_signature(_current_bid->get_owner()), base_id ).get_address() );
                             if( !bid_payout )
                                bid_payout = balance_record( _current_bid->get_owner(), asset(0,base_id), 0 );
                             bid_payout->balance += xts_received_by_bid.amount;
                             _pending_state->store_balance_record( *bid_payout );
                             _pending_state->store_bid_record( _current_bid->market_index, _current_bid->state );
                          }
                          else if( _current_bid->type == short_order )
                          {
                             // TODO: what if the amount paid is 0 for bid and ask due to rounding errors,
                             // make sure this doesn't put us in an infinite loop.
                             if( quote_quantity == _current_bid->get_quote_quantity() )
                                _current_bid->state.balance = 0;
                             else
                                _current_bid->state.balance -= xts_received_by_bid.amount;

                             auto collateral = ((usd_paid_by_bid * _current_bid->get_price()) + xts_paid_by_ask).amount;
                             auto cover_price = usd_received_by_ask / asset( (3*collateral)/4, base_id );

                             market_index_key cover_index( cover_price, _current_ask->get_owner() );
                             auto ocover_record = _pending_state->get_collateral_record( cover_index );

                             if( NOT ocover_record )
                                ocover_record = collateral_record();

                             ocover_record->collateral_balance += collateral;
                             ocover_record->payoff_balance += usd_received_by_ask.amount;
                             _pending_state->store_collateral_record( cover_index, *ocover_record );

                             _pending_state->store_short_record( _current_bid->market_index, _current_bid->state );
                          }

                       }

                       if( trading_volume.amount > 0 && last_valid_bid.ratio != price::infinite() && last_valid_ask.ratio != price::infinite() )
                       {
                         market_history_key key(quote_id, base_id, market_history_key::each_block, _db_impl._head_block_header.timestamp);
                         market_history_record new_record(last_valid_bid, last_valid_ask, trading_volume.amount);
                         //LevelDB iterators are dumb and don't support proper past-the-end semantics.
                         auto last_key_itr = _db_impl._market_history_db.lower_bound(key);
                         if( !last_key_itr.valid() )
                           last_key_itr = _db_impl._market_history_db.last();
                         else
                           --last_key_itr;

                         key.timestamp = timestamp;

                         //Unless the previous record for this market is the same as ours...
                         if( (!(last_key_itr.valid()
                             && last_key_itr.key().quote_id == quote_id
                             && last_key_itr.key().base_id == base_id
                             && last_key_itr.key().granularity == market_history_key::each_block
                             && last_key_itr.value() == new_record)) )
                         {
                           //...add a new entry to the history table.
                           wlog("Adding new price point: ${key} ${rec}", ("key",key)("rec",new_record));
                           _pending_state->market_history[key] = new_record;
                         }

                         fc::time_point_sec start_of_this_hour(timestamp.sec_since_epoch() % (60*60));
                         market_history_key old_key(quote_id, base_id, market_history_key::each_hour, start_of_this_hour);
                         if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
                         {
                           auto old_record = *opt;
                           old_record.volume += new_record.volume;
                           if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                           {
                             old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                             old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                             _pending_state->market_history[old_key] = old_record;
                           }
                         }
                         else
                           _pending_state->market_history[old_key] = new_record;

                         fc::time_point_sec start_of_this_day(timestamp.sec_since_epoch() % (60*60*24));
                         old_key = market_history_key(quote_id, base_id, market_history_key::each_day, start_of_this_day);
                         if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
                         {
                           auto old_record = *opt;
                           old_record.volume += new_record.volume;
                           if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                           {
                             old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                             old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                             _pending_state->market_history[old_key] = old_record;
                           }
                         }
                         else
                           _pending_state->market_history[old_key] = new_record;
                       }

                       wlog( "done matching orders" );
                  } FC_CAPTURE_AND_RETHROW( (quote_id)(base_id) ) }

                  bool get_next_bid()
                  { try {
                     if( _current_bid && _current_bid->get_quantity().amount > 0 ) 
                        return _current_bid.valid();

                     _current_bid.reset();
                     if( _bid_itr.valid() )
                     {
                        auto bid = market_order( bid_order, _bid_itr.key(), _bid_itr.value() );
                        if( bid.get_price().quote_asset_id == _quote_id && 
                            bid.get_price().base_asset_id == _base_id )
                        {
                            _current_bid = bid;
                        }
                     }

                     if( _short_itr.valid() )
                     {
                        auto bid = market_order( short_order, _short_itr.key(), _short_itr.value() );
                        wlog( "SHORT ITER VALID: ${o}", ("o",bid) );
                        if( bid.get_price().quote_asset_id == _quote_id && 
                            bid.get_price().base_asset_id == _base_id )
                        {
                            if( !_current_bid || _current_bid->get_price() < bid.get_price() )
                            {
                               --_short_itr;
                               _current_bid = bid;
                               ilog( "returning ${v}", ("v",_current_bid.valid() ) );
                               return _current_bid.valid();
                            }
                        }
                     }
                     else
                     {
                        wlog( "           No Shorts         ****   " );
                     }
                     if( _bid_itr.valid() ) --_bid_itr;
                     return _current_bid.valid();
                  } FC_CAPTURE_AND_RETHROW() }

                  bool get_next_ask()
                  { try {
                     if( _current_ask && _current_ask->state.balance > 0 )
                     {
                        wlog( "current ask" );
                        return _current_ask.valid();
                     }
                     _current_ask.reset();

                     /**
                      *  Margin calls take priority over all other ask orders
                      */
                     while( _current_bid && _collateral_itr.valid() )
                     {
                        auto cover_ask = market_order( cover_order,
                                                 _collateral_itr.key(), 
                                                 order_record(_collateral_itr.value().payoff_balance), 
                                                 _collateral_itr.value().collateral_balance  );

                        if( cover_ask.get_price().quote_asset_id == _quote_id &&
                            cover_ask.get_price().base_asset_id == _base_id )
                        {
                            if( _current_bid->get_price() < cover_ask.get_highest_cover_price()  )
                            {
                               // cover position has been blown out, current bid is not able to
                               // cover the position, so it will sit until the price recovers 
                               // enough to fill it.  
                               //
                               // The idea here is that the longs have agreed to a maximum 
                               // protection equal to the collateral.  If they would like to
                               // sell their USD for XTS this is the best price the short is
                               // obligated to offer.  
                               --_collateral_itr;
                               continue;
                            }
                            // max bid must be greater than call price
                            if( _current_bid->get_price() < cover_ask.get_price() )
                            {
                             //  if( _current_ask->get_price() > cover_ask.get_price() )
                               {
                                  _current_ask = cover_ask;
                                  _current_payoff_balance = _collateral_itr.value().payoff_balance;
                                  --_collateral_itr;
                                  return _current_ask.valid();
                               }
                            }
                        }
                        break;
                     }

                     if( _ask_itr.valid() )
                     {
                        auto ask = market_order( ask_order, _ask_itr.key(), _ask_itr.value() );
                        wlog( "ASK ITER VALID: ${o}", ("o",ask) );
                        if( ask.get_price().quote_asset_id == _quote_id && 
                            ask.get_price().base_asset_id == _base_id )
                        {
                            _current_ask = ask;
                        }
                        ++_ask_itr;
                        return true;
                     }
                     return _current_ask.valid();
                  } FC_CAPTURE_AND_RETHROW() }

                  pending_chain_state_ptr     _pending_state;
                  chain_database_impl&        _db_impl;
                  
                  optional<market_order>      _current_bid;
                  optional<market_order>      _current_ask;
                  share_type                  _current_payoff_balance;
                  asset_id_type               _quote_id;
                  asset_id_type               _base_id;

                  vector<market_transaction>  _market_transactions;

                  bts::db::level_map< market_index_key, order_record >::iterator       _bid_itr;
                  bts::db::level_map< market_index_key, order_record >::iterator       _ask_itr;
                  bts::db::level_map< market_index_key, order_record >::iterator       _short_itr;
                  bts::db::level_map< market_index_key, collateral_record >::iterator  _collateral_itr;
            };

            void                                        initialize_genesis(fc::optional<fc::path> genesis_file);


            std::pair<block_id_type, block_fork_data>   store_and_index( const block_id_type& id, const full_block& blk );
            void                                        clear_pending(  const full_block& blk );
            void                                        switch_to_fork( const block_id_type& block_id );
            void                                        extend_chain( const full_block& blk );
            vector<block_id_type>                       get_fork_history( const block_id_type& id );
            void                                        pop_block();
            void                                        mark_invalid( const block_id_type& id, const fc::exception& reason );
            void                                        mark_included( const block_id_type& id, bool state );
            void                                        verify_header( const full_block& );
            void                                        apply_transactions( const signed_block_header& block,
                                                                            const std::vector<signed_transaction>&,
                                                                            const pending_chain_state_ptr& );
            void                                        pay_delegate( const block_id_type& block_id, const pending_chain_state_ptr& );
            void                                        save_undo_state( const block_id_type& id,
                                                                            const pending_chain_state_ptr& );
            void                                        update_head_block( const full_block& blk );
            std::vector<block_id_type> fetch_blocks_at_number( uint32_t block_num );
            std::pair<block_id_type, block_fork_data>   recursive_mark_as_linked( const std::unordered_set<block_id_type>& ids );
            void                                        recursive_mark_as_invalid( const std::unordered_set<block_id_type>& ids, const fc::exception& reason );

            void                                        execute_markets(const fc::time_point_sec& timestamp, const pending_chain_state_ptr& pending_state );
            void                                        update_random_seed( secret_hash_type new_secret,
                                                                           const pending_chain_state_ptr& pending_state );
            void                                        update_active_delegate_list(const full_block& block_data,
                                                                                    const pending_chain_state_ptr& pending_state );

            void                                        update_delegate_production_info( const full_block& block_data,
                                                                                         const pending_chain_state_ptr& pending_state );

            void                                        revalidate_pending()
            {
                  _pending_fee_index.clear();

                  vector<transaction_id_type> trx_to_discard;

                  _pending_trx_state = std::make_shared<pending_chain_state>( self->shared_from_this() );
                  auto itr = _pending_transaction_db.begin();
                  while( itr.valid() )
                  {
                     auto trx = itr.value();
                     auto trx_id = trx.id();
                     try {
                        auto eval_state = self->evaluate_transaction( trx, _priority_fee );
                        share_type fees = eval_state->get_fees();
                        _pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
                        _pending_transaction_db.store( trx_id, trx );
                     } 
                     catch ( const fc::exception& e )
                     {
                        trx_to_discard.push_back(trx_id);
                        wlog( "discarding invalid transaction: ${id} ${e}",
                              ("id",trx_id)("e",e.to_detail_string()) );
                     }
                     ++itr;
                  }

                  for( const auto& item : trx_to_discard )
                     _pending_transaction_db.remove( item );
            }
            fc::future<void> _revalidate_pending;
            fc::mutex        _push_block_mutex;
      
            /**
             *  Used to track the cumulative effect of all pending transactions that are known,
             *  new incomming transactions are evaluated relative to this state.
             *
             *  After a new block is pushed this state is recalculated based upon what ever
             *  pending transactions remain.
             */
            pending_chain_state_ptr                                         _pending_trx_state;

            chain_database*                                                 self;
            unordered_set<chain_observer*>                                  _observers;
            digest_type                                                     _chain_id;
            bool                                                            _skip_signature_verification;
            share_type                                                      _priority_fee;

            bts::db::level_map<uint32_t, std::vector<market_transaction> >  _market_transactions_db;
            bts::db::level_map<slate_id_type, delegate_slate >              _slate_db;
            bts::db::level_map<uint32_t, std::vector<block_id_type> >       _fork_number_db;
            bts::db::level_map<block_id_type,block_fork_data>               _fork_db;
            bts::db::level_map<uint32_t, fc::variant >                      _property_db;
            bts::db::level_map<proposal_id_type, proposal_record >          _proposal_db;
            bts::db::level_map<proposal_vote_id_type, proposal_vote >       _proposal_vote_db;

            /** the data required to 'undo' the changes a block made to the database */
            bts::db::level_map<block_id_type,pending_chain_state>           _undo_state_db;

            // blocks in the current 'official' chain.
            bts::db::level_map<uint32_t,block_id_type >                     _block_num_to_id_db;
            // all blocks from any fork..
            //bts::db::level_map<block_id_type,full_block>                  _block_id_to_block_db;
            bts::db::level_map<block_id_type,block_record >                 _block_id_to_block_record_db;

            bts::db::level_map<block_id_type,full_block>                    _block_id_to_block_data_db;

            std::unordered_set<transaction_id_type>                         _known_transactions;
            bts::db::level_map<transaction_id_type,transaction_record>      _id_to_transaction_record_db;

            // used to revert block state in the event of a fork
            // bts::db::level_map<uint32_t,undo_data>                       _block_num_to_undo_data_db;

            signed_block_header                                             _head_block_header;
            block_id_type                                                   _head_block_id;

            bts::db::level_map< transaction_id_type, signed_transaction>    _pending_transaction_db;
            std::map< fee_index, transaction_evaluation_state_ptr >         _pending_fee_index;

            bts::db::level_map< asset_id_type, asset_record >               _asset_db;
            bts::db::level_map< balance_id_type, balance_record>            _balance_db;
            bts::db::level_map< account_id_type, account_record>            _account_db;
            bts::db::level_map< address, account_id_type >                  _address_to_account_db;

            bts::db::level_map< string, account_id_type >                   _account_index_db;
            bts::db::level_map< string, asset_id_type >                     _symbol_index_db;
            bts::db::level_map< vote_del, int >                             _delegate_vote_index_db;

            bts::db::level_map< time_point_sec, slot_record >               _slot_record_db;

            bts::db::level_map< market_index_key, order_record >            _ask_db;
            bts::db::level_map< market_index_key, order_record >            _bid_db;
            bts::db::level_map< market_index_key, order_record >            _short_db;
            bts::db::level_map< market_index_key, collateral_record >       _collateral_db;

            bts::db::level_map< market_history_key, market_history_record>  _market_history_db;

            /** used to prevent duplicate processing */
            // bts::db::level_pod_map< transaction_id_type, transaction_location > _processed_transaction_id_db;

            void open_database(const fc::path& data_dir );
      };

      void chain_database_impl::open_database( const fc::path& data_dir )
      { try {
          bool rebuild_index = false;

          if( !fc::exists(data_dir / "index" ) )
          {
              ilog("Rebuilding database index...");
              fc::create_directories( data_dir / "index" );
              rebuild_index = true;
          }

          _property_db.open( data_dir / "index/property_db" );
          auto database_version = _property_db.fetch_optional( chain_property_enum::database_version );
          if( !database_version || database_version->as_int64() < BTS_BLOCKCHAIN_DATABASE_VERSION )
          {
              if ( !rebuild_index )
              {
                wlog( "old database version, upgrade and re-sync" );
                _property_db.close();
                fc::remove_all( data_dir / "index" );
                fc::create_directories( data_dir / "index" );
                _property_db.open( data_dir / "index/property_db" );
                rebuild_index = true;
              }
              self->set_property( chain_property_enum::database_version, BTS_BLOCKCHAIN_DATABASE_VERSION );
              self->set_property( chain_property_enum::current_fee_rate, BTS_BLOCKCHAIN_MIN_FEE );
              self->set_property( chain_property_enum::accumulated_fees, 0 );
              self->set_property( chain_property_enum::dirty_markets, variant( map<asset_id_type,asset_id_type>() ) );
          }
          else if( database_version && !database_version->is_null() && database_version->as_int64() > BTS_BLOCKCHAIN_DATABASE_VERSION )
          {
             FC_CAPTURE_AND_THROW( new_database_version, (database_version)(BTS_BLOCKCHAIN_DATABASE_VERSION) ); 
          }
          _market_transactions_db.open( data_dir / "index/market_transactions_db" );
          _fork_number_db.open( data_dir / "index/fork_number_db" );
          _fork_db.open( data_dir / "index/fork_db" );
          _slate_db.open( data_dir / "index/slate_db" );
          _proposal_db.open( data_dir / "index/proposal_db" );
          _proposal_vote_db.open( data_dir / "index/proposal_vote_db" );

          _undo_state_db.open( data_dir / "index/undo_state_db" );

          _block_num_to_id_db.open( data_dir / "index/block_num_to_id_db" );
          _block_id_to_block_record_db.open( data_dir / "index/block_id_to_block_record_db" );
          _block_id_to_block_data_db.open( data_dir / "raw_chain/block_id_to_block_data_db" );
          _id_to_transaction_record_db.open( data_dir / "index/id_to_transaction_record_db" );

          for( auto itr = _id_to_transaction_record_db.begin(); itr.valid(); ++itr )
             _known_transactions.insert( itr.key() );

          _pending_transaction_db.open( data_dir / "index/pending_transaction_db" );

          _asset_db.open( data_dir / "index/asset_db" );
          _balance_db.open( data_dir / "index/balance_db" );
          _account_db.open( data_dir / "index/account_db" );
          _address_to_account_db.open( data_dir / "index/address_to_account_db" );

          _account_index_db.open( data_dir / "index/account_index_db" );
          _symbol_index_db.open( data_dir / "index/symbol_index_db" );
          _delegate_vote_index_db.open( data_dir / "index/delegate_vote_index_db" );

          _slot_record_db.open( data_dir / "index/slot_record_db" );

          _ask_db.open( data_dir / "index/ask_db" );
          _bid_db.open( data_dir / "index/bid_db" );
          _short_db.open( data_dir / "index/short_db" );
          _collateral_db.open( data_dir / "index/collateral_db" );

          _market_history_db.open( data_dir / "index/market_history_db" );

          _pending_trx_state = std::make_shared<pending_chain_state>( self->shared_from_this() );
      } FC_CAPTURE_AND_RETHROW( (data_dir) ) }

      std::vector<block_id_type> chain_database_impl::fetch_blocks_at_number( uint32_t block_num )
      {
         std::vector<block_id_type> current_blocks;
         auto itr = _fork_number_db.find( block_num );
         if( itr.valid() ) return itr.value();
         return current_blocks;
      }

      void chain_database_impl::clear_pending(  const full_block& blk )
      {
         std::unordered_set<transaction_id_type> confirmed_trx_ids;

         for( const auto& trx : blk.user_transactions )
         {
            auto id = trx.id();
            confirmed_trx_ids.insert( id );
            _pending_transaction_db.remove( id );
         }

         _pending_fee_index.clear();

         if( _revalidate_pending.valid() ) 
         {
            try {
            _revalidate_pending.cancel();
            _revalidate_pending.wait();
            } catch ( ... ) {}
         }
         // schedule the revalidating of pending transactions for 1 second in the future so that we don't hold
         // up block validation / propagation
         //revalidate_pending();
         _revalidate_pending = fc::async( [=](){ revalidate_pending(); } );//, fc::time_point::now() + fc::seconds(1) );

         _pending_trx_state = std::make_shared<pending_chain_state>( self->shared_from_this() );
      }

      std::pair<block_id_type, block_fork_data> chain_database_impl::recursive_mark_as_linked( const std::unordered_set<block_id_type>& ids )
      {
         block_fork_data longest_fork;
         uint32_t highest_block_num = 0;
         block_id_type last_block_id;

         std::unordered_set<block_id_type> next_ids = ids;
         while( next_ids.size() )
         {
            std::unordered_set<block_id_type> pending;
            for( const auto& item : next_ids )
            {
                block_fork_data record = _fork_db.fetch( item );
                record.is_linked = true;
                pending.insert( record.next_blocks.begin(), record.next_blocks.end() );
                //ilog( "store: ${id} => ${data}", ("id",item)("data",record) );
                _fork_db.store( item, record );

                auto block_record = _block_id_to_block_record_db.fetch(item);
                if( block_record.block_num > highest_block_num )
                {
                    highest_block_num = block_record.block_num;
                    last_block_id = item;
                    longest_fork = record;
                }
            }
            next_ids = pending;
         }

         return std::make_pair(last_block_id, longest_fork);
      }
      void chain_database_impl::recursive_mark_as_invalid(const std::unordered_set<block_id_type>& ids , const fc::exception& reason)
      {
         std::unordered_set<block_id_type> next_ids = ids;
         while( next_ids.size() )
         {
            std::unordered_set<block_id_type> pending;
            for( const auto& item : next_ids )
            {
                block_fork_data record = _fork_db.fetch( item );
                record.is_valid = false;
                record.invalid_reason = reason;
                pending.insert( record.next_blocks.begin(), record.next_blocks.end() );
                //ilog( "store: ${id} => ${data}", ("id",item)("data",record) );
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
          auto now = blockchain::now();
          //ilog( "block_number: ${n}   id: ${id}  prev: ${prev}",
          //      ("n",block_data.block_num)("id",block_id)("prev",block_data.previous) );

          // first of all store this block at the given block number
          _block_id_to_block_data_db.store( block_id, block_data );

          if( !self->get_block_record( block_id ).valid() ) /* Only insert with latency if not already present */
          {
              auto latency = now - block_data.timestamp;
              block_record record( block_data, self->get_current_random_seed(), block_data.block_size(), latency );
              _block_id_to_block_record_db.store( block_id, record );
          }

          // update the parallel block list
          vector<block_id_type> parallel_blocks = fetch_blocks_at_number( block_data.block_num );
          if (std::find( parallel_blocks.begin(), parallel_blocks.end(), block_id ) == parallel_blocks.end())
          {
            // don't add the block to the list if it's already there.
            // TODO: do we need to execute any of the rest of this function (or, for that matter, its caller) if the block is already there
            parallel_blocks.push_back( block_id );
            _fork_number_db.store( block_data.block_num, parallel_blocks );
          }


          // now find how it links in.
          block_fork_data prev_fork_data;
          auto prev_itr = _fork_db.find( block_data.previous );
          if( prev_itr.valid() ) // we already know about its previous
          {
             ilog( "           we already know about its previous: ${p}", ("p",block_data.previous) );
             prev_fork_data = prev_itr.value();
             prev_fork_data.next_blocks.insert(block_id);
             //ilog( "              ${id} = ${record}", ("id",prev_itr.key())("record",prev_fork_data) );
             _fork_db.store( prev_itr.key(), prev_fork_data );
          }
          else
          {
             elog( "           we don't know about its previous: ${p}", ("p",block_data.previous) );
             
             // create it... we do not know about the previous block so
             // we must create it and assume it is not linked...
             prev_fork_data.next_blocks.insert(block_id);
             prev_fork_data.is_linked = block_data.previous == block_id_type(); //false;
             //ilog( "              ${id} = ${record}", ("id",block_data.previous)("record",prev_fork_data) );
             _fork_db.store( block_data.previous, prev_fork_data );
          }

          block_fork_data current_fork;
          auto cur_itr = _fork_db.find( block_id );
          current_fork.is_known = true;
          if( cur_itr.valid() )
          {
             current_fork = cur_itr.value();
             ilog( "          current_fork: ${fork}", ("fork",current_fork) );
             ilog( "          prev_fork: ${prev_fork}", ("prev_fork",prev_fork_data) );
             if( !current_fork.is_linked && prev_fork_data.is_linked )
             {
                // we found the missing link
                current_fork.is_linked = true;
                auto longest_fork = recursive_mark_as_linked( current_fork.next_blocks );
                _fork_db.store( block_id, current_fork );
                return longest_fork;
             }
          }
          else
          {
             current_fork.is_linked = prev_fork_data.is_linked;
             //ilog( "          current_fork: ${id} = ${fork}", ("id",block_id)("fork",current_fork) );
          }
          _fork_db.store( block_id, current_fork );
          return std::make_pair(block_id, current_fork);
      } FC_CAPTURE_AND_RETHROW( (block_id) ) }

      void chain_database_impl::mark_invalid(const block_id_type& block_id , const fc::exception& reason)
      {
         // fetch the fork data for block_id, mark it as invalid and
         // then mark every item after it as invalid as well.
         auto fork_data = _fork_db.fetch( block_id );
         fork_data.is_valid = false;
         fork_data.invalid_reason = reason;
         _fork_db.store( block_id, fork_data );
         recursive_mark_as_invalid( fork_data.next_blocks, reason );
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
         ilog( "switch from fork ${id} to ${to_id}", ("id",_head_block_id)("to_id",block_id) );
         vector<block_id_type> history = get_fork_history( block_id );
         FC_ASSERT( history.size() > 0 );
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

      void chain_database_impl::apply_transactions( const signed_block_header& block,
                                                    const std::vector<signed_transaction>& user_transactions,
                                                    const pending_chain_state_ptr& pending_state )
      {
         //ilog( "apply transactions from block: ${block_num}  ${trxs}", ("block_num",block.block_num)("trxs",user_transactions) );
         ilog( "Applying transactions from block: ${n}", ("n",block.block_num) );
         uint32_t trx_num = 0;
         try {
            share_type total_fees = 0;
            // apply changes from each transaction
            for( const auto& trx : user_transactions )
            {
               //ilog( "applying   ${trx}", ("trx",trx) );
               transaction_evaluation_state_ptr trx_eval_state =
                      std::make_shared<transaction_evaluation_state>(pending_state,_chain_id);
               trx_eval_state->evaluate( trx, _skip_signature_verification );
               //ilog( "evaluation: ${e}", ("e",*trx_eval_state) );
               // TODO:  capture the evaluation state with a callback for wallets...
               // summary.transaction_states.emplace_back( std::move(trx_eval_state) );
               

               transaction_location trx_loc( block.block_num, trx_num );
               //ilog( "store trx location: ${loc}", ("loc",trx_loc) );
               transaction_record record( trx_loc, *trx_eval_state);
               pending_state->store_transaction( trx.id(), record );
               ++trx_num;

               // TODO: remove this once performance picks up, but for now
               // we don't want to block very long, so we yield a bit
               if( trx_num % 20 == 19 ) 
                  fc::usleep( fc::microseconds( 2000 ) );

               total_fees += record.get_fees();
            }
            /* Collect fees in block record */
            auto block_id = block.id();
            auto block_record = self->get_block_record( block_id );
            FC_ASSERT( block_record.valid() );
            block_record->total_fees += total_fees;
            _block_id_to_block_record_db.store( block_id, *block_record );

            auto prev_accumulated_fees = pending_state->get_accumulated_fees();
            pending_state->set_accumulated_fees( prev_accumulated_fees + total_fees );

      } FC_RETHROW_EXCEPTIONS( warn, "", ("trx_num",trx_num) ) }

      void chain_database_impl::pay_delegate( const block_id_type& block_id,
                                              const pending_chain_state_ptr& pending_state )
      { try {
            auto delegate_record = pending_state->get_account_record( self->get_block_signee( block_id ).id );
            FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
            auto pay_rate = delegate_record->delegate_info->pay_rate; 
            FC_ASSERT( pay_rate <= 100 );
            auto pay = (pay_rate*pending_state->get_delegate_pay_rate())/100;

            auto prev_accumulated_fees = pending_state->get_accumulated_fees();
            pending_state->set_accumulated_fees( prev_accumulated_fees - pay );

            delegate_record->delegate_info->pay_balance += pay;
            delegate_record->delegate_info->votes_for += pay;
            pending_state->store_account_record( *delegate_record );

            auto base_asset_record = pending_state->get_asset_record( asset_id_type(0) );
            FC_ASSERT( base_asset_record.valid() );
            base_asset_record->current_share_supply += pay;
            pending_state->store_asset_record( *base_asset_record );
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }

      void chain_database_impl::save_undo_state( const block_id_type& block_id,
                                                 const pending_chain_state_ptr& pending_state )
      { try {
           pending_chain_state_ptr undo_state = std::make_shared<pending_chain_state>(nullptr);
           pending_state->get_undo_state( undo_state );
           _undo_state_db.store( block_id, *undo_state );
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",block_id) ) }


      void chain_database_impl::verify_header( const full_block& block_data )
      { try {
            // validate preliminaries:
            if( block_data.block_num != _head_block_header.block_num + 1 )
               FC_CAPTURE_AND_THROW( block_numbers_not_sequential, (block_data)(_head_block_header) );
            if( block_data.previous  != _head_block_id )
               FC_CAPTURE_AND_THROW( invalid_previous_block_id, (block_data)(_head_block_id) );
            if( block_data.timestamp.sec_since_epoch() % BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC != 0 )
               FC_CAPTURE_AND_THROW( invalid_block_time );
            if( block_data.timestamp <= _head_block_header.timestamp )
               FC_CAPTURE_AND_THROW( time_in_past, (block_data.timestamp)(_head_block_header.timestamp) );

            fc::time_point_sec now = bts::blockchain::now();
            auto delta_seconds = (block_data.timestamp - now).to_seconds();
            if( block_data.timestamp >  (now + BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC*2) )
                FC_CAPTURE_AND_THROW( time_in_future, (block_data.timestamp)(now)(delta_seconds) );


            // TODO: move to state update
            //auto   expected_next_fee = block_data.next_fee( self->get_fee_rate(),  block_size );

            digest_block digest_data(block_data);
            if( NOT digest_data.validate_digest() )
              FC_CAPTURE_AND_THROW( invalid_block_digest );

            FC_ASSERT( digest_data.validate_unique() );

            // signing delegate:
            auto expected_delegate = self->get_slot_signee( block_data.timestamp, self->get_active_delegates() );

            if( NOT block_data.validate_signee( expected_delegate.active_key() ) )
               FC_CAPTURE_AND_THROW( invalid_delegate_signee, (expected_delegate.id) );
      } FC_CAPTURE_AND_RETHROW( (block_data) ) }

      void chain_database_impl::update_head_block( const full_block& block_data )
      {
         _head_block_header = block_data;
         _head_block_id = block_data.id();
      }

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
      void chain_database_impl::update_delegate_production_info( const full_block& produced_block,
                                                                 const pending_chain_state_ptr& pending_state )
      {
          /* Update production info for signing delegate */

          auto delegate_id = self->get_block_signee( produced_block.id() ).id;
          auto delegate_record = pending_state->get_account_record( delegate_id );
          FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );

          /* Validate secret */
          if( delegate_record->delegate_info->blocks_produced > 0 )
          {
              auto hash_of_previous_secret = fc::ripemd160::hash( produced_block.previous_secret );
              FC_ASSERT( hash_of_previous_secret == delegate_record->delegate_info->next_secret_hash,
                         "",
                         ("previous_secret",produced_block.previous_secret)
                         ("hash_of_previous_secret",hash_of_previous_secret)
                         ("delegate_record",delegate_record) );
          }

          delegate_record->delegate_info->blocks_produced += 1;
          delegate_record->delegate_info->next_secret_hash = produced_block.next_secret_hash;
          delegate_record->delegate_info->last_block_num_produced = produced_block.block_num;
          pending_state->store_account_record( *delegate_record );

          auto slot = slot_record( produced_block.timestamp, delegate_id, true, produced_block.id() );
          pending_state->store_slot_record( slot );

          /* Update production info for missing delegates */

          auto required_confirmations = pending_state->get_property( confirmation_requirement ).as_int64();

          time_point_sec block_timestamp;
          auto head_block = self->get_head_block();
          if( head_block.block_num > 0 ) block_timestamp = head_block.timestamp + BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
          else block_timestamp = produced_block.timestamp;
          const auto& active_delegates = self->get_active_delegates();

          for( ; block_timestamp < produced_block.timestamp;
                 block_timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC,
                 required_confirmations += 2 )
          {
              /* Note: Active delegate list has not been updated yet so we can use the timestamp */
              delegate_id = self->get_slot_signee( block_timestamp, active_delegates ).id;
              delegate_record = pending_state->get_account_record( delegate_id );
              FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );

              delegate_record->delegate_info->blocks_missed += 1;
              pending_state->store_account_record( *delegate_record );

              pending_state->store_slot_record( slot_record( block_timestamp, delegate_id )  );
          }

          /* Update required confirmation count */

          required_confirmations -= 1;
          if( required_confirmations < 1 ) required_confirmations = 1;
          if( required_confirmations > BTS_BLOCKCHAIN_NUM_DELEGATES*3 )
             required_confirmations = 3*BTS_BLOCKCHAIN_NUM_DELEGATES;

          pending_state->set_property( confirmation_requirement, required_confirmations );
      }

      void chain_database_impl::update_random_seed( secret_hash_type new_secret, 
                                                    const pending_chain_state_ptr& pending_state )
      {
         auto current_seed = pending_state->get_current_random_seed();
         fc::sha512::encoder enc;
         fc::raw::pack( enc, new_secret );
         fc::raw::pack( enc, current_seed );
         pending_state->set_property( last_random_seed_id, 
                                      fc::variant(fc::ripemd160::hash( enc.result() )) );
      }
       
      void chain_database_impl::update_active_delegate_list( const full_block& block_data, 
                                                             const pending_chain_state_ptr& pending_state )
      {
          if( block_data.block_num % BTS_BLOCKCHAIN_NUM_DELEGATES == 0 )
          {
             // perform a random shuffle of the sorted delegate list.
             
             auto active_del = self->next_round_active_delegates();
             auto rand_seed = fc::sha256::hash(self->get_current_random_seed());
             size_t num_del = active_del.size();
             for( uint32_t i = 0; i < num_del; ++i )
             {
                for( uint32_t x = 0; x < 4 && i < num_del; ++x, ++i )
                   std::swap( active_del[i], active_del[rand_seed._hash[x]%num_del] );
                rand_seed = fc::sha256::hash(rand_seed);
             }

             pending_state->set_property( chain_property_enum::active_delegate_list_id, fc::variant(active_del) );
          }
      }
      void chain_database_impl::execute_markets( const fc::time_point_sec& timestamp, const pending_chain_state_ptr& pending_state )
      { try { 
        elog( "execute markets ${e}", ("e", pending_state->get_dirty_markets()) );
        map<asset_id_type,share_type> collected_fees;

        vector<market_transaction> market_transactions;
        for( const auto& market_pair : pending_state->get_dirty_markets() )
        {
           FC_ASSERT( market_pair.first > market_pair.second ) 
           market_engine engine( pending_state, *this );
           engine.execute( market_pair.first, market_pair.second, timestamp );
           market_transactions.insert( market_transactions.end(), engine._market_transactions.begin(), engine._market_transactions.end() );
        } 
        pending_state->set_dirty_markets( pending_state->_dirty_markets );
        pending_state->set_market_transactions( std::move( market_transactions ) );
      } FC_CAPTURE_AND_RETHROW() }

      /**
       *  Performs all of the block validation steps and throws if error.
       */
      void chain_database_impl::extend_chain( const full_block& block_data )
      { try {
         auto block_id = block_data.id();
         block_summary summary;
         try
         {
            /* Note: Secret is validated later in update_delegate_production_info() */
            verify_header( block_data );

            summary.block_data = block_data;

            /* Create a pending state to track changes that would apply as we evaluate the block */
            pending_chain_state_ptr pending_state = std::make_shared<pending_chain_state>( self->shared_from_this() );
            summary.applied_changes = pending_state;

            /** Increment the blocks produced or missed for all delegates. This must be done
             *  before applying transactions because it depends upon the current active delegate order.
             **/
            update_delegate_production_info( block_data, pending_state );

            // apply any deterministic operations such as market operations before we perturb indexes
            //apply_deterministic_updates(pending_state);
            
            pay_delegate( block_data.id(), pending_state );

            apply_transactions( block_data, block_data.user_transactions, pending_state );

            execute_markets( block_data.timestamp, pending_state );

            update_active_delegate_list( block_data, pending_state );

            update_random_seed( block_data.previous_secret, pending_state );

            // update fee rate
            pending_state->set_fee_rate( block_data.next_fee( pending_state->get_fee_rate(), block_data.block_size() ) );

            save_undo_state( block_id, pending_state );

            // TODO: verify that apply changes can be called any number of
            // times without changing the database other than the first
            // attempt.
            pending_state->apply_changes();

            mark_included( block_id, true );

            update_head_block( block_data );

            clear_pending( block_data );

            _block_num_to_id_db.store( block_data.block_num, block_id );

            // self->sanity_check();
         }
         catch ( const fc::exception& e )
         {
            wlog( "error applying block: ${e}", ("e",e.to_detail_string() ));
            mark_invalid( block_id, e );
            throw;
         }

         // just in case something changes while calling observer
         for( const auto& o : _observers )
         {
            try { 
               o->block_applied( summary );
            } catch ( const fc::exception& e )
            {
               wlog( "${e}", ("e",e.to_detail_string() ) );
            }
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
      } FC_RETHROW_EXCEPTIONS( warn, "", ("block_id",id) ) }

      void  chain_database_impl::pop_block()
      { try {
         if( _head_block_header.block_num == 0 )
         {
            wlog( "attempting to pop block 0" );
            return;
         }

           // update the is_included flag on the fork data
         mark_included( _head_block_id, false );

         // update the block_num_to_block_id index
         _block_num_to_id_db.remove( _head_block_header.block_num );

         auto previous_block_id = _head_block_header.previous;

         bts::blockchain::pending_chain_state_ptr undo_state = std::make_shared<bts::blockchain::pending_chain_state>(_undo_state_db.fetch( _head_block_id ));
         undo_state->set_prev_state( self->shared_from_this() );
         undo_state->apply_changes();

         _head_block_id = previous_block_id;
         _head_block_header = self->get_block_header( _head_block_id );

         for( const auto& o : _observers )
             o->state_changed( undo_state );
      } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   } // namespace detail

   chain_database::chain_database()
   :my( new detail::chain_database_impl() )
   {
      my->self = this;
      my->_skip_signature_verification = true;
      my->_priority_fee = BTS_BLOCKCHAIN_DEFAULT_PRIORITY_FEE;
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

   std::vector<account_id_type> chain_database::next_round_active_delegates()const
   {
      return get_delegates_by_vote( 0, BTS_BLOCKCHAIN_NUM_DELEGATES );
   }

   /**
    *  @return the top BTS_BLOCKCHAIN_NUM_DELEGATES by vote
    */
   std::vector<account_id_type> chain_database::get_delegates_by_vote(uint32_t first, uint32_t count )const
   { try {
      auto del_vote_itr = my->_delegate_vote_index_db.begin();
      std::vector<account_id_type> sorted_delegates;
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
   std::vector<account_record> chain_database::get_delegate_records_by_vote(uint32_t first, uint32_t count )const
   { try {
      auto del_vote_itr = my->_delegate_vote_index_db.begin();
      std::vector<account_record> sorted_delegates;
      uint32_t pos = 0;
      while( sorted_delegates.size() < count && del_vote_itr.valid() )
      {
         if( pos >= first )
            sorted_delegates.push_back( *get_account_record(del_vote_itr.value()) );
         ++pos;
         ++del_vote_itr;
      }
      return sorted_delegates;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void chain_database::open( const fc::path& data_dir, fc::optional<fc::path> genesis_file )
   { try {
      wlog( "....." );
      bool is_new_data_dir = !fc::exists( data_dir );
      try
      {
          fc::create_directories( data_dir );

          my->open_database( data_dir );

          // TODO: check to see if we crashed during the last write
          //   if so, then apply the last undo operation stored.

          uint32_t       last_block_num = -1;
          block_id_type  last_block_id;
          my->_block_num_to_id_db.last( last_block_num, last_block_id );
          if( last_block_num != uint32_t(-1) )
          {
             my->_head_block_header = get_block_digest( last_block_id );
             my->_head_block_id = last_block_id;
          }


          if( last_block_num == uint32_t(-1) )
          {
             close();
             fc::remove_all( data_dir / "index" );
             fc::create_directories( data_dir / "index");
             my->open_database( data_dir );
             my->initialize_genesis(genesis_file);

             std::cout << "Please be patient, this could take a few minutes.\n\rRe-indexing database... [/]" << std::flush;

             const char spinner[] = "-\\|/";
             int blocks_indexed = 0;

             auto start_time = blockchain::now();
             auto block_itr = my->_block_id_to_block_data_db.begin();
             while( block_itr.valid() )
             {
                 std::cout << "\rRe-indexing database... [" << spinner[blocks_indexed++ % 4] << "]" << std::flush;

                 auto block = block_itr.value();
                 ++block_itr;

                 push_block(block);
             }
             std::cout << "\rSuccessfully re-indexed " << blocks_indexed << " blocks in " << (blockchain::now() - start_time).to_seconds() << " seconds.\n" << std::flush;
          }
          my->_chain_id = get_property( bts::blockchain::chain_id ).as<digest_type>();


          //  process the pending transactions to cache by fees
          auto pending_itr = my->_pending_transaction_db.begin();
          wlog( "loading pending trx..." );
          while( pending_itr.valid() )
          {
             try {
                auto trx = pending_itr.value();
                wlog( " laoding pending transaction ${trx}", ("trx",trx) );
                auto trx_id = trx.id();
                auto eval_state = evaluate_transaction( trx, my->_priority_fee );
                share_type fees = eval_state->get_fees();
                my->_pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
                my->_pending_transaction_db.store( trx_id, trx );
             }
             catch ( const fc::exception& e )
             {
                wlog( "error processing pending transaction: ${e}", ("e",e.to_detail_string() ) );
             }
             ++pending_itr;
          }
      }
      catch( ... )
      {
          elog( "error opening database" );
          close();
          if( is_new_data_dir ) fc::remove_all( data_dir );
          throw;
      }

   } FC_RETHROW_EXCEPTIONS( warn, "", ("data_dir",data_dir) ) }

   void chain_database::close()
   { try {
      my->_market_transactions_db.close();
      my->_fork_number_db.close();
      my->_fork_db.close();
      my->_slate_db.close();
      my->_property_db.close();
      my->_proposal_db.close();
      my->_proposal_vote_db.close();

      my->_undo_state_db.close();

      my->_block_num_to_id_db.close();
      my->_block_id_to_block_record_db.close();
      my->_block_id_to_block_data_db.close();
      my->_id_to_transaction_record_db.close();

      my->_pending_transaction_db.close();

      my->_asset_db.close();
      my->_balance_db.close();
      my->_account_db.close();
      my->_address_to_account_db.close();

      my->_account_index_db.close();
      my->_symbol_index_db.close();
      my->_delegate_vote_index_db.close();

      my->_slot_record_db.close();

      my->_ask_db.close();
      my->_bid_db.close();
      my->_short_db.close();
      my->_collateral_db.close();

      my->_market_history_db.close();

      //my->_processed_transaction_id_db.close();
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

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

   account_record chain_database::get_slot_signee( const time_point_sec& timestamp,
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

   transaction_evaluation_state_ptr chain_database::evaluate_transaction( const signed_transaction& trx, const share_type& required_fees )
   { try {
      if( !my->_pending_trx_state )
         my->_pending_trx_state = std::make_shared<pending_chain_state>( shared_from_this() );

      pending_chain_state_ptr          pend_state = std::make_shared<pending_chain_state>(my->_pending_trx_state);
      transaction_evaluation_state_ptr trx_eval_state = std::make_shared<transaction_evaluation_state>(pend_state,my->_chain_id);

      trx_eval_state->evaluate( trx );
      auto fees = trx_eval_state->get_fees();
      if( fees < required_fees )
         FC_CAPTURE_AND_THROW( insufficient_priority_fee, (fees)(required_fees) );

      // apply changes from this transaction to _pending_trx_state
      pend_state->apply_changes();

      return trx_eval_state;
   } FC_CAPTURE_AND_RETHROW( (trx) ) }

   optional<fc::exception> chain_database::get_transaction_error( const signed_transaction& transaction, const share_type& min_fee )
   { try {
       try
       {
          auto pending_state = std::make_shared<pending_chain_state>( shared_from_this() );
          transaction_evaluation_state_ptr eval_state = std::make_shared<transaction_evaluation_state>( pending_state, my->_chain_id );

          eval_state->evaluate( transaction );
          auto fees = eval_state->get_fees();
          if( fees < min_fee )
             FC_CAPTURE_AND_THROW( insufficient_priority_fee, (fees)(min_fee) );
       }
       catch( fc::exception& e )
       {
           return e;
       }
       return optional<fc::exception>();
   } FC_CAPTURE_AND_RETHROW( (transaction) ) }

   signed_block_header  chain_database::get_block_header( const block_id_type& block_id )const
   { try {
      return *get_block_record( block_id );
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   signed_block_header  chain_database::get_block_header( uint32_t block_num )const
   { try {
      return *get_block_record( get_block_id( block_num ) );
   } FC_CAPTURE_AND_RETHROW( (block_num) ) }

   oblock_record chain_database::get_block_record( const block_id_type& block_id ) const
   { try {
      return my->_block_id_to_block_record_db.fetch_optional(block_id);
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
   {
      auto block_record = my->_block_id_to_block_record_db.fetch(block_id);
      vector<transaction_record> result;
      result.reserve( block_record.user_transaction_ids.size() );

      for( const auto& trx_id : block_record.user_transaction_ids )
      {
         auto otrx_record = get_transaction( trx_id );
         if( !otrx_record ) FC_CAPTURE_AND_THROW( unknown_transaction, (trx_id) );
         result.emplace_back( *otrx_record );
      }
      return result;
   }
   digest_block  chain_database::get_block_digest( const block_id_type& block_id )const
   {
      return my->_block_id_to_block_record_db.fetch(block_id);
   }
   digest_block  chain_database::get_block_digest( uint32_t block_num )const
   {
      auto block_id = my->_block_num_to_id_db.fetch( block_num );
      return get_block_digest( block_id );
   }

   full_block           chain_database::get_block( const block_id_type& block_id )const
   { try {
      return my->_block_id_to_block_data_db.fetch(block_id);
   } FC_CAPTURE_AND_RETHROW( (block_id) ) }

   full_block           chain_database::get_block( uint32_t block_num )const
   { try {
      auto block_id = my->_block_num_to_id_db.fetch( block_num );
      return get_block( block_id );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("block_num",block_num) ) }

   signed_block_header  chain_database::get_head_block()const
   {
      return my->_head_block_header;
   }

   /**
    *  Adds the block to the database and manages any reorganizations as a result.
    *
    *  Returns the block_fork_data of the new block, not necessarily the head block
    */
   block_fork_data chain_database::push_block( const full_block& block_data )
   { try {
      // only allow a single fiber attempt to push blocks at any given time,
      // this method is not re-entrant.
      fc::unique_lock<fc::mutex> lock( my->_push_block_mutex );

      auto processing_start_time = time_point::now();
      auto block_id = block_data.id();
      auto current_head_id = my->_head_block_id;

      std::pair<block_id_type, block_fork_data> longest_fork = my->store_and_index( block_id, block_data );
      optional<block_fork_data> new_fork_data = get_block_fork_data(block_id);
      FC_ASSERT(new_fork_data, "can't get fork data for a block we just successfully pushed");

      //ilog( "previous ${p} ==? current ${c}", ("p",block_data.previous)("c",current_head_id) );
      if( block_data.previous == current_head_id )
      {
         // attempt to extend chain
         my->extend_chain( block_data );
         new_fork_data = get_block_fork_data(block_id);
         FC_ASSERT(new_fork_data, "can't get fork data for a block we just successfully pushed");
      }
      else if( longest_fork.second.can_link() &&
               my->_block_id_to_block_record_db.fetch(longest_fork.first).block_num > my->_head_block_header.block_num )
      {
         try {
            my->switch_to_fork( longest_fork.first );
            new_fork_data = get_block_fork_data(block_id);
            FC_ASSERT(new_fork_data, "can't get fork data for a block we just successfully pushed");
         }
         catch ( const fc::exception& e )
         {
            wlog( "attempt to switch to fork failed: ${e}, reverting", ("e",e.to_detail_string() ) );
            my->switch_to_fork( current_head_id );
         }
      }

      /* Store processing time */
      auto record = get_block_record( block_id );
      FC_ASSERT( record.valid() );
      record->processing_time = time_point::now() - processing_start_time;
      my->_block_id_to_block_record_db.store( block_id, *record );

      return *new_fork_data;
   } FC_CAPTURE_AND_RETHROW( (block_data) )  }

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

   oasset_record chain_database::get_asset_record( asset_id_type id )const
   {
      auto itr = my->_asset_db.find( id );
      if( itr.valid() )
      {
         return itr.value(); 
      }
      return oasset_record();
   }

   oaccount_record chain_database::get_account_record( const address& account_owner )const
   { try {
      auto itr = my->_address_to_account_db.find( account_owner );
      if( itr.valid() )
         return get_account_record( itr.value() );
      return oaccount_record();
   } FC_CAPTURE_AND_RETHROW( (account_owner) ) }

   obalance_record chain_database::get_balance_record( const balance_id_type& balance_id )const
   {
      return my->_balance_db.fetch_optional( balance_id );
   }

   oaccount_record chain_database::get_account_record( account_id_type account_id )const
   { try {
      return my->_account_db.fetch_optional( account_id );
   } FC_CAPTURE_AND_RETHROW( (account_id) ) }

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
   
   oasset_record chain_database::get_asset_record( const string& symbol )const
   { try {
       auto symbol_id_itr = my->_symbol_index_db.find( symbol );
       if( symbol_id_itr.valid() )
          return get_asset_record( symbol_id_itr.value() );
       return oasset_record();
   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

   oaccount_record chain_database::get_account_record( const string& account_name )const
   { try {
       auto account_id_itr = my->_account_index_db.find( account_name );
       if( account_id_itr.valid() )
          return get_account_record( account_id_itr.value() );
       return oaccount_record();
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   void chain_database::store_asset_record( const asset_record& asset_to_store )
   { try {
       if( asset_to_store.is_null() )
       {
          my->_asset_db.remove( asset_to_store.id );
          my->_symbol_index_db.remove( asset_to_store.symbol );
       }
       else
       {
          my->_asset_db.store( asset_to_store.id, asset_to_store );
          my->_symbol_index_db.store( asset_to_store.symbol, asset_to_store.id );
       }
   } FC_CAPTURE_AND_RETHROW( (asset_to_store) ) }

   void chain_database::store_balance_record( const balance_record& r )
   { try {
       wlog( "balance record: ${r}", ("r",r) );
       if( r.is_null() )
       {
          my->_balance_db.remove( r.id() );
       }
       else
       {
          my->_balance_db.store( r.id(), r );
       }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("record", r) ) }

   void chain_database::store_account_record( const account_record& record_to_store )
   { try {
       oaccount_record old_rec = get_account_record( record_to_store.id );

       if( record_to_store.is_null() && old_rec)
       {
          my->_account_db.remove( record_to_store.id );
          my->_account_index_db.remove( record_to_store.name );

          for( const auto& item : old_rec->active_key_history )
             my->_address_to_account_db.remove( address(item.second) );

          if( old_rec->is_delegate() )
          {
              auto itr = my->_delegate_vote_index_db.begin();
              while( itr.valid() )
              {
                  ++itr;
              }
              my->_delegate_vote_index_db.remove( vote_del( old_rec->net_votes(), 
                                                            record_to_store.id ) );
              itr = my->_delegate_vote_index_db.begin();
              while( itr.valid() )
              {
                  ++itr;
              }
          }
       }
       else if( !record_to_store.is_null() )
       {
          my->_account_db.store( record_to_store.id, record_to_store );
          my->_account_index_db.store( record_to_store.name, record_to_store.id );

          for( const auto& item : record_to_store.active_key_history )
          { // re-index all keys for this record
             my->_address_to_account_db.store( address(item.second), record_to_store.id );
          }


          if( old_rec.valid() && old_rec->is_delegate() )
          {
              my->_delegate_vote_index_db.remove( vote_del( old_rec->net_votes(), 
                                                            record_to_store.id ) );
          }


          if( record_to_store.is_delegate() )
          {
              my->_delegate_vote_index_db.store( vote_del( record_to_store.net_votes(),
                                                           record_to_store.id ), 
                                                0/*dummy value*/ );
          }
       }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("record", record_to_store) ) }

   otransaction_record chain_database::get_transaction( const transaction_id_type& trx_id, bool exact )const
   { try {
      auto trx_rec = my->_id_to_transaction_record_db.fetch_optional( trx_id );
      if( trx_rec || exact )
      {
         //ilog( "trx_rec: ${id} => ${t}", ("id",trx_id)("t",trx_rec) );
         if( trx_rec )
            FC_ASSERT( trx_rec->trx.id() == trx_id,"", ("trx_rec->id",trx_rec->trx.id()) );
         return trx_rec;
      }
      
      ilog( "... lower bound...?" );
      auto itr = my->_id_to_transaction_record_db.lower_bound( trx_id );
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
      if( record_to_store.trx.operations.size() == 0 )
      {
        my->_id_to_transaction_record_db.remove( record_id );
        my->_known_transactions.erase( record_id );
      }
      else
      {
        FC_ASSERT( record_id == record_to_store.trx.id() );
        my->_id_to_transaction_record_db.store( record_id, record_to_store );
        my->_known_transactions.insert( record_id );
      }
   } FC_CAPTURE_AND_RETHROW( (record_id)(record_to_store) ) }

   void chain_database::scan_assets( function<void( const asset_record& )> callback )
   {
        auto asset_itr = my->_asset_db.begin();
        while( asset_itr.valid() )
        {
           callback( asset_itr.value() );
           ++asset_itr;
        }
   }

   void chain_database::scan_balances( function<void( const balance_record& )> callback )
   {
        auto balances = my->_balance_db.begin();
        while( balances.valid() )
        {
           callback( balances.value() );
           ++balances;
        }
   }

   void chain_database::scan_accounts( function<void( const account_record& )> callback )
   {
        auto name_itr = my->_account_db.begin();
        while( name_itr.valid() )
        {
           callback( name_itr.value() );
           ++name_itr;
        }
   }

   /** this should throw if the trx is invalid */
   transaction_evaluation_state_ptr chain_database::store_pending_transaction( const signed_transaction& trx, bool override_limits )
   { try {

      auto trx_id = trx.id();
      auto current_itr = my->_pending_transaction_db.find( trx_id );
      if( current_itr.valid() ) return nullptr;

      share_type priority_fee = my->_priority_fee;
      if( !override_limits )
      {
         if( my->_pending_fee_index.size() > BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE )
         {
             auto overage = my->_pending_fee_index.size() - BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE;
             priority_fee = my->_priority_fee * overage * overage; 
         }
      }

      auto eval_state = evaluate_transaction( trx, priority_fee );
      share_type fees = eval_state->get_fees();

      //if( fees < my->_priority_fee ) 
      //   FC_CAPTURE_AND_THROW( insufficient_priority_fee, (fees)(my->_priority_fee) );

      my->_pending_fee_index[ fee_index( fees, trx_id ) ] = eval_state;
      my->_pending_transaction_db.store( trx_id, trx );

      return eval_state;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx) ) }

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

   full_block chain_database::generate_block( const time_point_sec& timestamp )
   { try {
      auto start_time = time_point::now();

      pending_chain_state_ptr pending_state = std::make_shared<pending_chain_state>( shared_from_this() );
      auto pending_trx = get_pending_transactions();

      full_block next_block;
      size_t block_size = 0;
      share_type total_fees = 0;

      // TODO: Sort pending transactions by highest fee
      for( const auto& item : pending_trx )
      {
         auto trx_size = item->trx.data_size();
         if( block_size + trx_size > BTS_BLOCKCHAIN_MAX_BLOCK_SIZE ) break;
         block_size += trx_size;

         /* Make modifications to temporary state */
         auto pending_trx_state = std::make_shared<pending_chain_state>( pending_state );
         auto trx_eval_state = std::make_shared<transaction_evaluation_state>( pending_trx_state, my->_chain_id );

         try
         {
            trx_eval_state->evaluate( item->trx );
            // TODO: what about fees in other currencies?
            total_fees += trx_eval_state->get_fees(0);
            /* Apply temporary state to block state */
            pending_trx_state->apply_changes();
            next_block.user_transactions.push_back( item->trx );
         }
         catch( const fc::exception& e )
         {
            wlog( "Pending transaction was found to be invalid in context of block\n ${trx} \n${e}",
                  ("trx",fc::json::to_pretty_string(item->trx))("e",e.to_detail_string()) );
         }

         /* Limit the time we spend evaluating transactions */
         if( time_point::now() - start_time > fc::seconds(5) )
            break;
      }

      auto head_block = get_head_block();

      next_block.previous           = head_block.block_num ? head_block.id() : block_id_type();
      next_block.block_num          = head_block.block_num + 1;
      next_block.timestamp          = timestamp;
      next_block.transaction_digest = digest_block( next_block ).calculate_transaction_digest();

      return next_block;
   } FC_CAPTURE_AND_RETHROW( (timestamp) ) }

   void detail::chain_database_impl::initialize_genesis(fc::optional<fc::path> genesis_file)
   { try {
      if( self->chain_id() != digest_type() )
      {
         self->sanity_check();
         ilog( "Genesis state already initialized" );
         return;
      }

      genesis_block_config config;
      if (genesis_file)
      {
        // this will only happen during testing
        std::cout << "Initializing genesis state from "<< genesis_file->generic_string() << "\n";
        FC_ASSERT( fc::exists( *genesis_file ), "Genesis file '${file}' was not found.", ("file", *genesis_file) );

        if( genesis_file->extension() == ".json" )
        {
           config = fc::json::from_file(*genesis_file).as<genesis_block_config>();
        }
        else if( genesis_file->extension() == ".dat" )
        {
           fc::ifstream in( *genesis_file );
           fc::raw::unpack( in, config );
        }
        else
        {
           FC_ASSERT( !"Invalid genesis format", " '${format}'", ("format",genesis_file->extension() ) );
        }
      }
      else
      {
        // this is the usual case
        std::cout << "Initializing genesis state from built-in genesis file\n";
        std::string genesis_file_contents = get_builtin_genesis_json_as_string();
        config = fc::json::from_string(genesis_file_contents).as<genesis_block_config>();
      }

      fc::sha256::encoder enc;
      fc::raw::pack( enc, config );
      _chain_id = enc.result();
      self->set_property( bts::blockchain::chain_id, fc::variant(_chain_id) );

      fc::uint128 total_unscaled = 0;
      for( const auto& item : config.balances ) total_unscaled += int64_t(item.second/1000);
      ilog( "Total unscaled: ${s}", ("s", total_unscaled) );

      std::vector<name_config> delegate_config;
      for( const auto& item : config.names )
      {
         if( item.is_delegate ) delegate_config.push_back( item );
      }

      FC_ASSERT( delegate_config.size() >= BTS_BLOCKCHAIN_NUM_DELEGATES,
                 "genesis.json does not contain enough initial delegates",
                 ("required",BTS_BLOCKCHAIN_NUM_DELEGATES)("provided",delegate_config.size()) );

      account_record god; god.id = 0; god.name = "god";
      self->store_account_record( god );

      fc::time_point_sec timestamp = config.timestamp;
      std::vector<account_id_type> delegate_ids;
      int32_t account_id = 1;
      for( const auto& name : config.names )
      {
         account_record rec;
         rec.id                = account_id;
         rec.name              = name.name;
         rec.owner_key         = name.owner;
         rec.set_active_key( timestamp, name.owner );
         rec.registration_date = timestamp;
         rec.last_update       = timestamp;
         if( name.is_delegate )
         {
            rec.delegate_info = delegate_stats();
            delegate_ids.push_back( account_id );
         }
         self->store_account_record( rec );
         ++account_id;
      }

      int64_t n = 0;
      for( const auto& item : config.balances )
      {
         ++n;

         fc::uint128 initial( int64_t(item.second/1000) );
         initial *= fc::uint128(int64_t(BTS_BLOCKCHAIN_INITIAL_SHARES));
         initial /= total_unscaled;

         balance_record initial_balance( item.first,
                                         asset( share_type( initial.low_bits() ), 0 ),
                                         0 /** not voting for anyone */
                                       );

         // in case of redundant balances
         auto cur = self->get_balance_record( initial_balance.id() );
         if( cur.valid() ) initial_balance.balance += cur->balance;
         initial_balance.genesis                    = true;
         initial_balance.last_update                = config.timestamp;
         self->store_balance_record( initial_balance );
      }

      asset total;
      auto itr = _balance_db.begin();
      while( itr.valid() )
      {
         auto ind = itr.value().get_balance();
         FC_ASSERT( ind.amount >= 0, "", ("record",itr.value()) );
         total += ind;
         ++itr;
      }

      int32_t asset_id = 0;
      asset_record base_asset;
      base_asset.id = asset_id;
      base_asset.symbol = BTS_BLOCKCHAIN_SYMBOL;
      base_asset.name = BTS_BLOCKCHAIN_NAME;
      base_asset.description = BTS_BLOCKCHAIN_DESCRIPTION;
      base_asset.public_data = variant("");
      base_asset.issuer_account_id = god.id;
      base_asset.precision = BTS_BLOCKCHAIN_PRECISION;
      base_asset.registration_date = timestamp;
      base_asset.last_update = timestamp;
      base_asset.current_share_supply = total.amount;
      base_asset.maximum_share_supply = BTS_BLOCKCHAIN_MAX_SHARES;
      base_asset.collected_fees = 0;
      self->store_asset_record( base_asset );

      for( const auto& asset : config.market_assets )
      {
         ++asset_id;
         asset_record rec;
         rec.id = asset_id;
         rec.symbol = asset.symbol;
         rec.name = asset.name;
         rec.description = asset.description;
         rec.public_data = variant("");
         rec.issuer_account_id = asset_record::market_issued_asset;
         rec.precision = asset.precision;
         rec.registration_date = timestamp;
         rec.last_update = timestamp;
         rec.current_share_supply = 0;
         rec.maximum_share_supply = BTS_BLOCKCHAIN_MAX_SHARES;
         rec.collected_fees = 0;
         self->store_asset_record( rec );
      }

      block_fork_data gen_fork;
      gen_fork.is_valid = true;
      gen_fork.is_included = true;
      gen_fork.is_linked = true;
      gen_fork.is_known = true;
      _fork_db.store( block_id_type(), gen_fork );

      self->set_property( chain_property_enum::active_delegate_list_id, fc::variant(self->next_round_active_delegates()) );
      self->set_property( chain_property_enum::last_asset_id, asset_id );
      self->set_property( chain_property_enum::last_proposal_id, 0 );
      self->set_property( chain_property_enum::last_account_id, uint64_t(config.names.size()) );
      self->set_property( chain_property_enum::last_random_seed_id, fc::variant(secret_hash_type()) );
      self->set_property( chain_property_enum::confirmation_requirement, BTS_BLOCKCHAIN_NUM_DELEGATES*2 );

      self->sanity_check();
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void chain_database::add_observer( chain_observer* observer )
   {
      my->_observers.insert(observer);
   }

   void chain_database::remove_observer( chain_observer* observer )
   {
      my->_observers.erase(observer);
   }

   bool chain_database::is_known_block( const block_id_type& block_id )const
   {
      auto fork_data = get_block_fork_data( block_id );
      return fork_data && fork_data->is_known;
   }
   bool chain_database::is_included_block( const block_id_type& block_id )const
   {
      auto fork_data = get_block_fork_data( block_id );
      return fork_data && fork_data->is_included;
   }
   optional<block_fork_data> chain_database::get_block_fork_data( const block_id_type& id )const
   {
      return my->_fork_db.fetch_optional(id); 
   }

   uint32_t chain_database::get_block_num( const block_id_type& block_id )const
   { try {
      if( block_id == block_id_type() )
         return 0;
      return my->_block_id_to_block_record_db.fetch( block_id ).block_num;
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to find block ${block_id}", ("block_id", block_id) ) }

    uint32_t chain_database::get_head_block_num()const
    {
       return my->_head_block_header.block_num;
    }

    block_id_type chain_database::get_head_block_id()const
    {
       return my->_head_block_id;
    }

    std::vector<account_record> chain_database::get_accounts( const string& first, uint32_t count )const
    { try {
       std::vector<account_record> names;
       auto itr = my->_account_index_db.begin();

       if( first.size() > 0 && isdigit(first[0]) )
       {
         int32_t skip = atoi(first.c_str()) - 1;

         while( skip-- > 0 && itr++.valid() );
       }
       else
       {
         itr = my->_account_index_db.lower_bound(first);
       }

       while( itr.valid() && names.size() < count )
       {
          names.push_back( *get_account_record( itr.value() ) );
          ++itr;
       }

       return names;
    } FC_RETHROW_EXCEPTIONS( warn, "", ("first",first)("count",count) )  }

    std::vector<asset_record> chain_database::get_assets( const string& first_symbol, uint32_t count )const
    { try {
       auto itr = my->_symbol_index_db.lower_bound(first_symbol);
       std::vector<asset_record> assets;
       while( itr.valid() && assets.size() < count )
       {
          assets.push_back( *get_asset_record( itr.value() ) );
          ++itr;
       }
       return assets;
    } FC_RETHROW_EXCEPTIONS( warn, "", ("first_symbol",first_symbol)("count",count) )  }

    std::string chain_database::export_fork_graph( uint32_t start_block, uint32_t end_block, const fc::path& filename )const
    {
      FC_ASSERT( start_block >= 0 );
      FC_ASSERT( end_block >= start_block );
      std::stringstream out;
      out << "digraph G { \n"; 
      out << "rankdir=LR;\n";
        
      bool first = true;
      fc::time_point_sec start_time;
      std::map<uint32_t, std::vector<block_record> > nodes_by_rank;
      //std::set<uint32_t> ranks_in_use;
      for( auto block_itr = my->_block_id_to_block_record_db.begin(); block_itr.valid(); ++block_itr )
      {
        block_record block_record = block_itr.value();
        if (first)
        {
          first = false;
          start_time = block_record.timestamp;
        }
        std::cout << block_record.block_num << "  start " << start_block << "  end " << end_block << "\n";
        if ( block_record.block_num >= start_block && block_record.block_num <= end_block )
        {
          unsigned rank = (unsigned)((block_record.timestamp - start_time).to_seconds() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

          //ilog( "${id} => ${r}", ("id",fork_itr.key())("r",fork_data) );
          nodes_by_rank[rank].push_back(block_record);
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
                        block_record fork_block = my->_block_id_to_block_record_db.fetch(forked_block_id);

                        fork.block_id = forked_block_id;
                        fork.latency = fork_block.latency;
                        fork.signing_delegate = get_block_signee( forked_block_id ).id;
                        fork.transaction_count = fork_block.user_transaction_ids.size();
                        fork.size = fork_block.block_size;
                        fork.timestamp = fork_block.timestamp;
                        fork.is_valid = fork_data.is_valid;
                        fork.invalid_reason = fork_data.invalid_reason;
                        fork.is_current_fork = fork_data.is_included;

                        forks.push_back(fork);
                    }

                    fork_blocks[get_block_num( iter.key() )] = forks;
                }
            }
            catch( ... )
            {
                wlog( "error fetching block num of block ${b} while building fork list", ("b",iter.key()));
                throw;
            }
        }
       
        return fork_blocks;
    }

    vector<slot_record> chain_database::get_delegate_slot_records( const account_id_type& delegate_id )const
    {
        vector<slot_record> slot_records;
        for( auto iter = my->_slot_record_db.begin(); iter.valid(); ++iter )
        {
            const auto slot_record = iter.value();
            if( slot_record.block_producer_id == delegate_id )
                slot_records.push_back( slot_record );
        }
        return slot_records;
    }

   fc::variant chain_database::get_property( chain_property_enum property_id )const
   { try {
      return my->_property_db.fetch( property_id );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("property_id",property_id) ) }

   void chain_database::set_property( chain_property_enum property_id, 
                                                     const fc::variant& property_value )
   {
      if( property_value.is_null() )
         my->_property_db.remove( property_id );
      else
         my->_property_db.store( property_id, property_value );
   }

   void chain_database::store_proposal_record( const proposal_record& r )
   {
      if( r.is_null() )
      {
         my->_proposal_db.remove( r.id );
      }
      else
      {
         my->_proposal_db.store( r.id, r );
      }
   }

   oproposal_record chain_database::get_proposal_record( proposal_id_type id )const
   {
      return my->_proposal_db.fetch_optional(id);
   }
                                                                                            
   void chain_database::store_proposal_vote( const proposal_vote& r )
   {
      if( r.is_null() )
      {
         my->_proposal_vote_db.remove( r.id );
      }
      else
      {
         my->_proposal_vote_db.store( r.id, r );
      }
   }

   oproposal_vote chain_database::get_proposal_vote( proposal_vote_id_type id )const
   {
      return my->_proposal_vote_db.fetch_optional(id);
   }

   digest_type chain_database::chain_id()const
   {
         return my->_chain_id;
   }

   std::vector<proposal_record> chain_database::get_proposals( uint32_t first, uint32_t count )const
   {
      std::vector<proposal_record> results;
      auto current_itr = my->_proposal_db.lower_bound( first );
      uint32_t found = 0;
      while( current_itr.valid() && found < count )
      {
         results.push_back( current_itr.value() );
         ++found;
         ++current_itr;
      }
      return results;
   }

   std::vector<proposal_vote> chain_database::get_proposal_votes( proposal_id_type proposal_id ) const
   {
      std::vector<proposal_vote> results;
      auto current_itr = my->_proposal_vote_db.lower_bound( proposal_vote_id_type(proposal_id,0) );
      while( current_itr.valid() )
      {
         if( current_itr.key().proposal_id != proposal_id )
            return results;

         results.push_back( current_itr.value() );
         ++current_itr;
      }
      return results;
   }

   fc::ripemd160 chain_database::get_current_random_seed()const
   {
      return get_property( last_random_seed_id ).as<fc::ripemd160>();
   }

   oorder_record chain_database::get_bid_record( const market_index_key&  key )const
   {
      return my->_bid_db.fetch_optional(key);
   }

   omarket_order chain_database::get_lowest_ask_record( asset_id_type quote_id, asset_id_type base_id ) 
   {
      omarket_order result;
      auto itr = my->_ask_db.lower_bound( market_index_key( price(0,quote_id,base_id) ) );
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
      wlog( "STORE ASK ${k} ${o}", ("k",key)("o",order) );
      if( order.is_null() )
         my->_ask_db.remove( key );
      else
         my->_ask_db.store( key, order );
   }

   void chain_database::store_short_record( const market_index_key& key, const order_record& order )
   {
      if( order.is_null() )
         my->_short_db.remove( key );
      else
         my->_short_db.store( key, order );
   }

   void chain_database::store_collateral_record( const market_index_key& key, const collateral_record& collateral ) 
   {
      if( collateral.is_null() )
         my->_collateral_db.remove( key );
      else
         my->_collateral_db.store( key, collateral );
   }

   string chain_database::get_asset_symbol( asset_id_type asset_id )const
   { try {
      auto asset_rec = get_asset_record( asset_id );
      FC_ASSERT( asset_rec.valid(), "Unknown Asset ID: ${id}", ("asset_id",asset_id) );
      return asset_rec->symbol;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("asset_id",asset_id) ) }

   time_point_sec chain_database::get_genesis_timestamp()const
   {
       return get_asset_record( asset_id_type() )->registration_date;
   }

   void chain_database::sanity_check()const
   { try {
      asset total;
      auto itr = my->_balance_db.begin();
      while( itr.valid() )
      {
         auto ind = itr.value().get_balance();
         if( ind.asset_id == 0 )
         {
            FC_ASSERT( ind.amount >= 0, "", ("record",itr.value()) );
            total += ind;
         }
         ++itr;
      }
      int64_t total_votes = 0;
      auto aitr = my->_account_db.begin();
      while( aitr.valid() )
      {
         auto v = aitr.value();
         if( v.is_delegate() )
         {
            total += asset(v.delegate_info->pay_balance);
            total_votes += v.delegate_info->votes_for;
         }
         ++aitr;
      }

//      FC_ASSERT( total_votes == total.amount, "", 
 //                ("total_votes",total_votes)
  //               ("total_shares",total) );
     
      auto ar = get_asset_record( asset_id_type(0) );
      FC_ASSERT( ar.valid() );
      FC_ASSERT( ar->current_share_supply == total.amount, "", ("ar",ar)("total",total)("delta",ar->current_share_supply-total.amount) );
      FC_ASSERT( ar->current_share_supply <= ar->maximum_share_supply );
      //std::cerr << "Total Balances: " << to_pretty_asset( total ) << "\n";
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   /**
    *   Calculates the percentage of blocks produced in the last 10 rounds as an average
    *   measure of the delegate participation rate.
    *
    *   @return a value betwee 0 and 100
    */
   double chain_database::get_average_delegate_participation()const
   {
      int32_t head_num = get_head_block_num();
      if( head_num < 1 ) return 0;
      auto now         = bts::blockchain::now();
      if( head_num <  BTS_BLOCKCHAIN_NUM_DELEGATES )
      {
         // what percent of the maximum total blocks that could have been produced 
         // have been produced.
         auto expected_blocks = (now - get_block_header( 1 ).timestamp).to_seconds() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC; 
         return 100*double(head_num) / expected_blocks;
      }
      else 
      {
         // if 10*N blocks ago is longer than 10*N*INTERVAL_SEC ago then we missed blocks, calculate
         // in terms of percentage time rather than percentage blocks.
         auto starting_time =  get_block_header( head_num - BTS_BLOCKCHAIN_NUM_DELEGATES ).timestamp;
         auto expected_production = (now - starting_time).to_seconds() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC; 
         return  100*double(BTS_BLOCKCHAIN_NUM_DELEGATES) / expected_production;
      }
   }

   optional<market_order> chain_database::get_market_bid( const market_index_key& key )const
   { try {
       auto market_itr  = my->_bid_db.find(key);
       if( market_itr.valid() )
          return market_order { bid_order, market_itr.key(), market_itr.value() };

       return optional<market_order>();
   } FC_CAPTURE_AND_RETHROW( (key) ) }

   vector<market_order> chain_database::get_market_bids( const string& quote_symbol, 
                                                          const string& base_symbol, 
                                                          uint32_t limit  )
   { try {
       auto quote_asset_id = get_asset_id( quote_symbol );
       auto base_asset_id  = get_asset_id( base_symbol );
       if( base_asset_id >= quote_asset_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quote_asset_id)(base_asset_id) );

       vector<market_order> results;
       auto market_itr  = my->_bid_db.lower_bound( market_index_key( price( 0, quote_asset_id, base_asset_id ) ) );
       while( market_itr.valid() )
       {
          auto key = market_itr.key();
          if( key.order_price.quote_asset_id == quote_asset_id &&
              key.order_price.base_asset_id == base_asset_id  )
          {
             results.push_back( {bid_order, key, market_itr.value()} );
          }
          else break;


          if( results.size() == limit ) 
             return results;

          ++market_itr;
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

   vector<market_order> chain_database::get_market_shorts( const string& quote_symbol, 
                                                          uint32_t limit  )
   { try {
       auto quote_asset_id = get_asset_id( quote_symbol );
       auto base_asset_id  = 0;
       if( base_asset_id >= quote_asset_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quote_asset_id)(base_asset_id) );

       vector<market_order> results;

       auto market_itr  = my->_short_db.lower_bound( market_index_key( price( 0, quote_asset_id, base_asset_id ) ) );
       while( market_itr.valid() )
       {
          auto key = market_itr.key();
          if( key.order_price.quote_asset_id == quote_asset_id &&
              key.order_price.base_asset_id == base_asset_id  )
          {
             results.push_back( {short_order, key, market_itr.value()} );
          }
          else
          {
             break;
          }

          if( results.size() == limit ) 
             return results;

          ++market_itr;
       }
       return results;
   } FC_CAPTURE_AND_RETHROW( (quote_symbol)(limit) ) }

   vector<market_order> chain_database::get_market_covers( const string& quote_symbol, uint32_t limit )
   { try {
       auto quote_asset_id = get_asset_id( quote_symbol );
       auto base_asset_id  = 0;
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
             results.push_back( {cover_order, 
                                 key, 
                                 order_record(market_itr.value().payoff_balance), 
                                 market_itr.value().collateral_balance } );
          }
          else
          {
             break;
          }

          if( results.size() == limit ) 
             return results;

          ++market_itr;
       }
       return results;
   } FC_CAPTURE_AND_RETHROW( (quote_symbol)(limit) ) }


   optional<market_order> chain_database::get_market_ask( const market_index_key& key )const
   { try {
       auto market_itr  = my->_ask_db.find(key);
       if( market_itr.valid() )
          return market_order { ask_order, market_itr.key(), market_itr.value() };

       return optional<market_order>();
   } FC_CAPTURE_AND_RETHROW( (key) ) }

   vector<market_order> chain_database::get_market_asks( const string& quote_symbol, 
                                                          const string& base_symbol, 
                                                          uint32_t limit  )
   { try {
       auto quote_asset_id = get_asset_id( quote_symbol );
       auto base_asset_id  = get_asset_id( base_symbol );
       if( base_asset_id >= quote_asset_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quote_asset_id)(base_asset_id) );

       vector<market_order> results;
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

          if( results.size() == limit ) 
             return results;

          ++market_itr;
       }
       ilog( "end of db" );
       return results;
   } FC_CAPTURE_AND_RETHROW( (quote_symbol)(base_symbol)(limit) ) }

   pending_chain_state_ptr chain_database::get_pending_state()const
   {
      return my->_pending_trx_state;
   }

   odelegate_slate chain_database::get_delegate_slate( slate_id_type id )const
   {
      return my->_slate_db.fetch_optional( id );
   }

   void chain_database::store_delegate_slate( slate_id_type id, const delegate_slate& slate ) 
   {
      if( slate.supported_delegates.size() == 0 )
         my->_slate_db.remove( id );
      else
         my->_slate_db.store( id, slate );
   }

   void chain_database::store_slot_record( const slot_record& r )
   {
      if( !r.block_produced || (r.block_id != block_id_type()) ) /* If in valid state */
         my->_slot_record_db.store( r.start_time, r );
      else
         my->_slot_record_db.remove( r.start_time );
   }

   oslot_record chain_database::get_slot_record( const time_point_sec& start_time )const
   {
     return my->_slot_record_db.fetch_optional( start_time );
   }

   void chain_database::store_market_history_record(const market_history_key& key, const market_history_record& record)
   {
     if( record.volume == 0 )
       my->_market_history_db.remove( key );
     else
       my->_market_history_db.store( key, record );
   }

   omarket_history_record chain_database::get_market_history_record(const market_history_key& key) const
   {
     return my->_market_history_db.fetch_optional( key );
   }

   market_history_points chain_database::get_market_price_history( asset_id_type quote_id,
                                                                   asset_id_type base_id,
                                                                   const fc::time_point& start_time,
                                                                   const fc::microseconds& duration,
                                                                   market_history_key::time_granularity_enum granularity)
   {
      time_point_sec end_time = start_time + duration;
      auto record_itr = my->_market_history_db.lower_bound( market_history_key(quote_id, base_id, granularity, start_time) );
      market_history_points history;

      while( record_itr.valid()
             && record_itr.key().quote_id == quote_id
             && record_itr.key().base_id == base_id
             && record_itr.key().granularity == granularity
             && record_itr.key().timestamp <= end_time )
      {
        history.push_back( std::make_pair(record_itr.key().timestamp, record_itr.value()) );
        ++record_itr;
      }

      return history;
   }

   bool chain_database::is_known_transaction( const transaction_id_type& id )
   {
      return my->_known_transactions.find( id ) != my->_known_transactions.end();
   }
   void chain_database::skip_signature_verification( bool state )
   {
      my->_skip_signature_verification = state;
   }

   void chain_database::set_priority_fee( share_type shares )
   {
      my->_priority_fee = shares;
   }

   share_type chain_database::get_priority_fee()
   {
      return my->_priority_fee;
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

   vector<market_transaction> chain_database::get_market_transactions( uint32_t block_num  )const
   {
      auto tmp = my->_market_transactions_db.fetch_optional(block_num);
      if( tmp ) return *tmp;
      return vector<market_transaction>();
   }


} } // bts::blockchain

FC_REFLECT( vote_del, (votes)(delegate_id) )
FC_REFLECT( fee_index, (_fees)(_trx) )
