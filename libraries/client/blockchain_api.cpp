#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>

#include <fc/thread/non_preemptable_scope_check.hpp>

namespace bts { namespace client { namespace detail {

vector<account_record> client_impl::blockchain_list_active_delegates( uint32_t first, uint32_t count )const
{
   if( first > 0 ) --first;
   FC_ASSERT( first < BTS_BLOCKCHAIN_NUM_DELEGATES );
   FC_ASSERT( first + count <= BTS_BLOCKCHAIN_NUM_DELEGATES );
   vector<account_id_type> all_delegate_ids = _chain_db->get_active_delegates();
   FC_ASSERT( all_delegate_ids.size() == BTS_BLOCKCHAIN_NUM_DELEGATES );
   vector<account_id_type> delegate_ids( all_delegate_ids.begin() + first, all_delegate_ids.begin() + first + count );

   vector<account_record> delegate_records;
   delegate_records.reserve( count );
   for( const auto& delegate_id : delegate_ids )
   {
      auto delegate_record = _chain_db->get_account_record( delegate_id );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
      delegate_records.push_back( *delegate_record );
   }
   return delegate_records;
}

vector<account_record> client_impl::blockchain_list_delegates( uint32_t first, uint32_t count )const
{
   if( first > 0 ) --first;
   vector<account_id_type> delegate_ids = _chain_db->get_delegates_by_vote( first, count );

   vector<account_record> delegate_records;
   delegate_records.reserve( delegate_ids.size() );
   for( const auto& delegate_id : delegate_ids )
   {
      auto delegate_record = _chain_db->get_account_record( delegate_id );
      FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
      delegate_records.push_back( *delegate_record );
   }
   return delegate_records;
}

vector<string> client_impl::blockchain_list_missing_block_delegates( uint32_t block_num )
{
   FC_ASSERT( _chain_db->get_statistics_enabled() );
   if (block_num == 0 || block_num == 1)
      return vector<string>();
   vector<string> delegates;
   const auto this_block = _chain_db->get_block_header( block_num );
   const auto prev_block = _chain_db->get_block_header( block_num - 1 );
   auto timestamp = prev_block.timestamp;
   timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
   while (timestamp != this_block.timestamp)
   {
      const auto slot_record = _chain_db->get_slot_record( timestamp );
      FC_ASSERT( slot_record.valid() );
      const auto delegate_record = _chain_db->get_account_record( slot_record->index.delegate_id );
      FC_ASSERT( delegate_record.valid() );
      delegates.push_back( delegate_record->name );
      timestamp += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
   }
   return delegates;
}

vector<block_record> client_impl::blockchain_list_blocks( uint32_t first, int32_t count )
{
   FC_ASSERT( count <= 1000 );
   FC_ASSERT( count >= -1000 );
   if (count == 0) return vector<block_record>();

   uint32_t total_blocks = _chain_db->get_head_block_num();
   FC_ASSERT( first <= total_blocks );

   int32_t increment = 1;

   //Normalize first and count if count < 0 and adjust count if necessary to not try to list nonexistent blocks
   if( count < 0 )
   {
      first = total_blocks - first;
      count *= -1;
      if( signed(first) - count < 0 )
         count = first;
      increment = -1;
   }
   else
   {
      if ( first == 0 )
         first = 1;
      if( first + count - 1 > total_blocks )
         count = total_blocks - first + 1;
   }

   vector<block_record> result;
   result.reserve( count );

   for( int32_t block_num = first; count; --count, block_num += increment )
   {
      auto record = _chain_db->get_block_record( block_num );
      FC_ASSERT( record.valid() );
      result.push_back( *record );
   }

   return result;
}

signed_transactions client_impl::blockchain_list_pending_transactions() const
{
   signed_transactions trxs;
   vector<transaction_evaluation_state_ptr> pending = _chain_db->get_pending_transactions();
   trxs.reserve(pending.size());
   for (const auto& trx_eval_ptr : pending)
   {
      trxs.push_back(trx_eval_ptr->trx);
   }
   return trxs;
}

block_id_type detail::client_impl::blockchain_get_block_hash( uint32_t block_number ) const
{
   return _chain_db->get_block(block_number).id();
}

uint32_t detail::client_impl::blockchain_get_block_count() const
{
   return _chain_db->get_head_block_num();
}

oaccount_record detail::client_impl::blockchain_get_account( const string& account )const
{
   try
   {
      ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
      if( std::all_of( account.begin(), account.end(), ::isdigit) )
         return _chain_db->get_account_record( std::stoi( account ) );
      else if( account.substr( 0, string( BTS_ADDRESS_PREFIX ).size() ) == BTS_ADDRESS_PREFIX ) {
         //Magic number 39 is hopefully longer than the longest address and shorter than the shortest key. Hopefully.
         if( account.length() < 39 )
            return _chain_db->get_account_record( address( account ) );
         else
            return _chain_db->get_account_record( address( blockchain::public_key_type( account ) ) );
      } else
         return _chain_db->get_account_record( account );
   }
   catch( ... )
   {
   }
   return oaccount_record();
}

map<account_id_type, string> detail::client_impl::blockchain_get_slate( const string& slate )const
{
    map<account_id_type, string> delegates;

    slate_id_type slate_id = 0;
    if( !std::all_of( slate.begin(), slate.end(), ::isdigit ) )
    {
        const oaccount_record account_record = _chain_db->get_account_record( slate );
        FC_ASSERT( account_record.valid() );
        try
        {
            FC_ASSERT( account_record->public_data.is_object() );
            const auto public_data = account_record->public_data.get_object();
            FC_ASSERT( public_data.contains( "slate_id" ) );
            FC_ASSERT( public_data[ "slate_id" ].is_uint64() );
            slate_id = public_data[ "slate_id" ].as<slate_id_type>();
        }
        catch( const fc::exception& )
        {
            return delegates;
        }
    }
    else
    {
        slate_id = std::stoi( slate );
    }

    const oslate_record slate_record = _chain_db->get_slate_record( slate_id );
    FC_ASSERT( slate_record.valid() );

    for( const account_id_type id : slate_record->slate )
    {
        const oaccount_record delegate_record = _chain_db->get_account_record( id );
        if( delegate_record.valid() )
        {
            if( delegate_record->is_delegate() ) delegates[ id ] = delegate_record->name;
            else delegates[ id ] = '(' + delegate_record->name + ')';
        }
        else
        {
            delegates[ id ] = std::to_string( id );
        }
    }

    return delegates;
}

balance_record detail::client_impl::blockchain_get_balance( const balance_id_type& balance_id )const
{
   const auto balance_record = _chain_db->get_balance_record( balance_id );
   FC_ASSERT( balance_record.valid() );
   return *balance_record;
}

oasset_record detail::client_impl::blockchain_get_asset( const string& asset )const
{
   try
   {
      ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
      if( !std::all_of( asset.begin(), asset.end(), ::isdigit ) )
         return _chain_db->get_asset_record( asset );
      else
         return _chain_db->get_asset_record( std::stoi( asset ) );
   }
   catch( ... )
   {
   }
   return oasset_record();
}

//TODO: Refactor: most of these next two functions are identical. Should extract a function.
vector<feed_entry> detail::client_impl::blockchain_get_feeds_for_asset(const std::string &asset) const
{ try {
      asset_id_type asset_id;
      if( !std::all_of( asset.begin(), asset.end(), ::isdigit) )
         asset_id = _chain_db->get_asset_id(asset);
      else
         asset_id = std::stoi( asset );

      auto raw_feeds = _chain_db->get_feeds_for_asset(asset_id, asset_id_type( 0 ));
      vector<feed_entry> result_feeds;
      for( auto feed : raw_feeds )
      {
         auto delegate = _chain_db->get_account_record(feed.index.delegate_id);
         if( !delegate )
            FC_THROW_EXCEPTION(unknown_account_id , "Unknown delegate", ("delegate_id", feed.index.delegate_id) );
         double price = _chain_db->to_pretty_price_double(feed.value);

         result_feeds.push_back({delegate->name, price, feed.last_update});
      }

      const auto omedian_price = _chain_db->get_active_feed_price( asset_id );
      if( omedian_price )
         result_feeds.push_back({"MARKET", 0, _chain_db->now(), _chain_db->get_asset_symbol(asset_id), _chain_db->to_pretty_price_double(*omedian_price)});

      return result_feeds;
} FC_RETHROW_EXCEPTIONS( warn, "", ("asset",asset) ) }

double detail::client_impl::blockchain_median_feed_price( const string& asset )const
{
   asset_id_type asset_id;
   if( !std::all_of( asset.begin(), asset.end(), ::isdigit) )
      asset_id = _chain_db->get_asset_id(asset);
   else
      asset_id = std::stoi( asset );
   const auto omedian_price = _chain_db->get_active_feed_price( asset_id );
   if( omedian_price )
      return _chain_db->to_pretty_price_double( *omedian_price );
   return 0;
}

vector<feed_entry> detail::client_impl::blockchain_get_feeds_from_delegate( const string& delegate_name )const
{ try {
      const auto delegate_record = _chain_db->get_account_record( delegate_name );
      if( !delegate_record.valid() || !delegate_record->is_delegate() )
         FC_THROW_EXCEPTION( unknown_account_name, "Unknown delegate account!" );

      const auto raw_feeds = _chain_db->get_feeds_from_delegate( delegate_record->id );
      vector<feed_entry> result_feeds;
      result_feeds.reserve( raw_feeds.size() );

      for( const auto& raw_feed : raw_feeds )
      {
         const double price = _chain_db->to_pretty_price_double( raw_feed.value );
         const string asset_symbol = _chain_db->get_asset_symbol( raw_feed.index.quote_id );
         const auto omedian_price = _chain_db->get_active_feed_price( raw_feed.index.quote_id );
         fc::optional<double> median_price;
         if( omedian_price )
            median_price = _chain_db->to_pretty_price_double( *omedian_price );

         result_feeds.push_back( feed_entry{ delegate_name, price, raw_feed.last_update, asset_symbol, median_price } );
      }

      return result_feeds;
} FC_RETHROW_EXCEPTIONS( warn, "", ("delegate_name", delegate_name) ) }

pair<transaction_id_type, transaction_record> detail::client_impl::blockchain_get_transaction( const string& transaction_id_prefix,
                                                                                               bool exact )const
{ try {
   const auto id_prefix = variant( transaction_id_prefix ).as<transaction_id_type>();
   const otransaction_record record = _chain_db->get_transaction( id_prefix, exact );
   FC_ASSERT( record.valid(), "Transaction not found!" );
   return std::make_pair( record->trx.id(), *record );
} FC_CAPTURE_AND_RETHROW( (transaction_id_prefix)(exact) ) }

oblock_record detail::client_impl::blockchain_get_block( const string& block )const
{
   try
   {
      ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
      if( block.size() == string( block_id_type() ).size() )
         return _chain_db->get_block_record( block_id_type( block ) );
      else
         return _chain_db->get_block_record( std::stoi( block ) );
   }
   catch( ... )
   {
   }
   return oblock_record();
}

map<balance_id_type, balance_record> detail::client_impl::blockchain_list_balances( const string& first, uint32_t limit )const
{ try {
    FC_ASSERT( limit > 0 );
    balance_id_type id;
    if( !first.empty() ) id = variant( first ).as<balance_id_type>();
    return _chain_db->get_balances( id, limit );
} FC_CAPTURE_AND_RETHROW( (first)(limit) ) }

account_balance_summary_type detail::client_impl::blockchain_get_account_public_balance( const string& account_name ) const
{ try {
  const auto& acct = _wallet->get_account( account_name );
  map<asset_id_type, share_type> balances;
  for( const auto& pair : _chain_db->get_balances_for_key( acct.active_key() ) )
      balances[pair.second.asset_id()] = pair.second.balance;
  account_balance_summary_type ret;
  ret[account_name] = balances;
  return ret;
} FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

map<balance_id_type, balance_record> detail::client_impl::blockchain_list_address_balances( const string& raw_addr, const time_point& after )const
{
    address addr;
    try {
        addr = address( raw_addr );
    } catch (...) {
        addr = address( pts_address( raw_addr ) );
    }
    auto result =  _chain_db->get_balances_for_address( addr );
    for( auto itr = result.begin(); itr != result.end(); )
    {
       if( fc::time_point(itr->second.last_update) <= after )
          itr = result.erase(itr);
       else
          ++itr;
    }
    return result;
}
fc::variant_object detail::client_impl::blockchain_list_address_transactions( const string& raw_addr,
                                                                              uint32_t after_block )const
{
   fc::mutable_variant_object results;

   address addr;
   try {
      addr = address( raw_addr );
   } catch (...) {
      addr = address( pts_address( raw_addr ) );
   }
   auto transactions = _chain_db->fetch_address_transactions( addr );
   ilog("Found ${num} transactions for ${addr}", ("num", transactions.size())("addr", raw_addr));

   if( after_block > 0 )
      transactions.erase(std::remove_if(transactions.begin(), transactions.end(),
                                        [after_block](const blockchain::transaction_record& t) -> bool {
         return t.chain_location.block_num <= after_block;
      }), transactions.end());
   ilog("Found ${num} transactions after block ${after_block}", ("num", transactions.size())("after_block", after_block));

   for( const auto& trx : transactions )
   {
      fc::mutable_variant_object bundle("timestamp", _chain_db->get_block(trx.chain_location.block_num).timestamp);
      bundle["trx"] = trx;
      results[string(trx.trx.id())] = bundle;
   }

   return results;
}

map<balance_id_type, balance_record> detail::client_impl::blockchain_list_key_balances( const public_key_type& key )const
{
    return _chain_db->get_balances_for_key( key );
}

vector<account_record> detail::client_impl::blockchain_list_accounts( const string& first, uint32_t limit )const
{ try {
   FC_ASSERT( limit > 0 );
   return _chain_db->get_accounts( first, limit );
} FC_CAPTURE_AND_RETHROW( (first)(limit) ) }

vector<account_record> detail::client_impl::blockchain_list_recently_updated_accounts()const
{
   FC_ASSERT( _chain_db->get_statistics_enabled() );
   vector<operation> account_updates = _chain_db->get_recent_operations(update_account_op_type);
   vector<account_record> accounts;
   accounts.reserve(account_updates.size());

   for( const operation& op : account_updates )
   {
      auto oaccount = _chain_db->get_account_record(op.as<update_account_operation>().account_id);
      if(oaccount)
         accounts.push_back(*oaccount);
   }

  return accounts;
}

vector<account_record> detail::client_impl::blockchain_list_recently_registered_accounts()const
{
   FC_ASSERT( _chain_db->get_statistics_enabled() );
   vector<operation> account_registrations = _chain_db->get_recent_operations(register_account_op_type);
   vector<account_record> accounts;
   accounts.reserve(account_registrations.size());

   for( const operation& op : account_registrations )
   {
      auto oaccount = _chain_db->get_account_record(op.as<register_account_operation>().owner_key);
      if(oaccount)
         accounts.push_back(*oaccount);
   }

   return accounts;
}

vector<asset_record> detail::client_impl::blockchain_list_assets( const string& first, uint32_t limit )const
{
   FC_ASSERT( limit > 0 );
   return _chain_db->get_assets( first, limit );
}

map<string, double> detail::client_impl::blockchain_list_feed_prices()const
{
    map<string, double> feed_prices;
    const auto scan_asset = [&]( const asset_record& record )
    {
        const auto median_price = _chain_db->get_active_feed_price( record.id );
        if( !median_price.valid() ) return;
        feed_prices.emplace( record.symbol, _chain_db->to_pretty_price_double( *median_price ) );
    };
    _chain_db->scan_ordered_assets( scan_asset );
    return feed_prices;
}

variant_object client_impl::blockchain_get_info()const
{
   auto info = fc::mutable_variant_object();

   info["blockchain_id"]                        = _chain_db->get_chain_id();

   info["name"]                                 = BTS_BLOCKCHAIN_NAME;
   info["symbol"]                               = BTS_BLOCKCHAIN_SYMBOL;
   info["address_prefix"]                       = BTS_ADDRESS_PREFIX;
   info["db_version"]                           = BTS_BLOCKCHAIN_DATABASE_VERSION;
   info["genesis_timestamp"]                    = _chain_db->get_genesis_timestamp();

   info["block_interval"]                       = BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
   info["delegate_num"]                         = BTS_BLOCKCHAIN_NUM_DELEGATES;
   info["max_delegate_pay_issued_per_block"]    = _chain_db->get_max_delegate_pay_issued_per_block();
   info["max_delegate_reg_fee"]                 = _chain_db->get_delegate_registration_fee( 100 );

   info["name_size_max"]                        = BTS_BLOCKCHAIN_MAX_NAME_SIZE;
   info["memo_size_max"]                        = BTS_BLOCKCHAIN_MAX_EXTENDED_MEMO_SIZE;
   info["data_size_max"]                        = BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE;

   info["symbol_size_max"]                      = BTS_BLOCKCHAIN_MAX_SUB_SYMBOL_SIZE;
   info["symbol_size_min"]                      = BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE;
   info["asset_shares_max"]                     = BTS_BLOCKCHAIN_MAX_SHARES;
   info["short_symbol_asset_reg_fee"]           = _chain_db->get_asset_registration_fee( BTS_BLOCKCHAIN_MIN_SYMBOL_SIZE );
   info["long_symbol_asset_reg_fee"]            = _chain_db->get_asset_registration_fee( BTS_BLOCKCHAIN_MAX_SUB_SYMBOL_SIZE );

   info["statistics_enabled"]                   = _chain_db->get_statistics_enabled();

   info["relay_fee"]                            = _chain_db->get_relay_fee();
   info["max_pending_queue_size"]               = BTS_BLOCKCHAIN_MAX_PENDING_QUEUE_SIZE;
   info["max_trx_per_second"]                   = BTS_BLOCKCHAIN_MAX_TRX_PER_SECOND;

   return info;
}

void client_impl::blockchain_generate_snapshot( const string& filename )const
{ try {
    _chain_db->generate_snapshot( fc::path( filename ) );
} FC_CAPTURE_AND_RETHROW( (filename) ) }

void client_impl::blockchain_generate_issuance_map( const string& symbol, const string& filename )const
{ try {
    _chain_db->generate_issuance_map( symbol, fc::path( filename ) );
} FC_CAPTURE_AND_RETHROW( (filename) ) }


asset client_impl::blockchain_calculate_supply( const string& asset )const
{
   asset_id_type asset_id;
   if( std::all_of( asset.begin(), asset.end(), ::isdigit ) )
      asset_id = std::stoi( asset );
   else
      asset_id = _chain_db->get_asset_id( asset );

   return _chain_db->calculate_supply( asset_id );
}

asset client_impl::blockchain_calculate_debt( const string& asset, bool include_interest )const
{
   asset_id_type asset_id;
   if( std::all_of( asset.begin(), asset.end(), ::isdigit ) )
      asset_id = std::stoi( asset );
   else
      asset_id = _chain_db->get_asset_id( asset );

   return _chain_db->calculate_debt( asset_id, include_interest );
}

bts::blockchain::blockchain_security_state client_impl::blockchain_get_security_state()const
{
   blockchain_security_state state;
   int64_t required_confirmations = _chain_db->get_required_confirmations();
   double participation_rate = _chain_db->get_average_delegate_participation();
   if( participation_rate > 100 ) participation_rate = 100;

   state.estimated_confirmation_seconds = (uint32_t)(required_confirmations * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
   state.participation_rate = participation_rate;
   if (!blockchain_is_synced())
   {
      state.alert_level = bts::blockchain::blockchain_security_state::grey;
   }
   else if (required_confirmations <= BTS_BLOCKCHAIN_NUM_DELEGATES / 2
            && participation_rate > 80)
   {
      state.alert_level = bts::blockchain::blockchain_security_state::green;
   }
   else if (required_confirmations > BTS_BLOCKCHAIN_NUM_DELEGATES
            || participation_rate < 60)
   {
      state.alert_level = bts::blockchain::blockchain_security_state::red;
   }
   else
   {
      state.alert_level = bts::blockchain::blockchain_security_state::yellow;
   }

   return state;
}

bool client_impl::blockchain_is_synced() const
{
   return (blockchain::now() - _chain_db->get_head_block().timestamp) < fc::seconds(BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
}

vector<market_order>    client_impl::blockchain_market_list_bids( const string& quote_symbol,
                                                                  const string& base_symbol,
                                                                  uint32_t limit  )
{
   return _chain_db->get_market_bids( quote_symbol, base_symbol, limit );
}
vector<market_order>    client_impl::blockchain_market_list_asks( const string& quote_symbol,
                                                                  const string& base_symbol,
                                                                  uint32_t limit  )
{
   return _chain_db->get_market_asks( quote_symbol, base_symbol, limit );
}

vector<market_order>    client_impl::blockchain_market_list_shorts( const string& quote_symbol,
                                                                    uint32_t limit  )const
{
   return _chain_db->get_market_shorts( quote_symbol, limit );
}
vector<market_order>    client_impl::blockchain_market_list_covers( const string& quote_symbol,
                                                                    const string& base_symbol,
                                                                    uint32_t limit  )
{
   return _chain_db->get_market_covers( quote_symbol, base_symbol, limit );
}

share_type              client_impl::blockchain_market_get_asset_collateral( const string& symbol )
{
   return _chain_db->get_asset_collateral( symbol );
}

std::pair<vector<market_order>,vector<market_order>> client_impl::blockchain_market_order_book( const string& quote_symbol,
                                                                                                const string& base_symbol,
                                                                                                uint32_t limit  )
{
   auto bids = blockchain_market_list_bids(quote_symbol, base_symbol, limit);
   auto asks = blockchain_market_list_asks(quote_symbol, base_symbol, limit);
   auto covers = blockchain_market_list_covers(quote_symbol, base_symbol, limit);
   asks.insert( asks.end(), covers.begin(), covers.end() );

   std::sort(bids.rbegin(), bids.rend(), [](const market_order& a, const market_order& b) -> bool {
      return a.market_index < b.market_index;
   });

   return std::make_pair(bids, asks);
}

std::vector<order_history_record> client_impl::blockchain_market_order_history( const std::string &quote_symbol,
                                                                                const std::string &base_symbol,
                                                                                uint32_t skip_count,
                                                                                uint32_t limit,
                                                                                const string& owner )const
{
   auto quote_id = _chain_db->get_asset_id(quote_symbol);
   auto base_id = _chain_db->get_asset_id(base_symbol);
   address owner_address = owner.empty()? address() : address(owner);

   return _chain_db->market_order_history(quote_id, base_id, skip_count, limit, owner_address);
}

market_history_points client_impl::blockchain_market_price_history( const std::string& quote_symbol,
                                                                    const std::string& base_symbol,
                                                                    const fc::time_point& start_time,
                                                                    const fc::microseconds& duration,
                                                                    const market_history_key::time_granularity_enum& granularity )const
{
   return _chain_db->get_market_price_history( _chain_db->get_asset_id(quote_symbol),
                                               _chain_db->get_asset_id(base_symbol),
                                               start_time, duration, granularity );
}

map<transaction_id_type, transaction_record> client_impl::blockchain_get_block_transactions( const string& block )const
{
   vector<transaction_record> transactions;
   if( block.size() == 40 )
      transactions = _chain_db->get_transactions_for_block( block_id_type( block ) );
   else
      transactions = _chain_db->get_transactions_for_block( _chain_db->get_block_id( std::stoi( block ) ) );

   map<transaction_id_type, transaction_record> transactions_map;
   for( const auto& transaction : transactions )
      transactions_map[ transaction.trx.id() ] = transaction;

   return transactions_map;
}

std::string client_impl::blockchain_export_fork_graph( uint32_t start_block, uint32_t end_block, const std::string& filename )const
{
   return _chain_db->export_fork_graph( start_block, end_block, filename );
}

std::map<uint32_t, vector<fork_record>> client_impl::blockchain_list_forks()const
{
   return _chain_db->get_forks_list();
}

vector<slot_record> client_impl::blockchain_get_delegate_slot_records( const string& delegate_name, uint32_t limit )const
{ try {
    FC_ASSERT( limit > 0 );
    const oaccount_record delegate_record = _chain_db->get_account_record( delegate_name );
    FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate(), "${n} is not a delegate!", ("n",delegate_name) );
    return _chain_db->get_delegate_slot_records( delegate_record->id, limit );
} FC_CAPTURE_AND_RETHROW( (delegate_name)(limit) ) }

string client_impl::blockchain_get_block_signee( const string& block )const
{
   if( block.size() == 40 )
      return _chain_db->get_block_signee( block_id_type( block ) ).name;
   else
      return _chain_db->get_block_signee( std::stoi( block ) ).name;
}

bool client_impl::blockchain_verify_signature(const string& signer, const fc::sha256& hash, const fc::ecc::compact_signature& signature) const
{
    try {
        auto key = public_key_type( signer );
        return key == fc::ecc::public_key(signature, hash);
    }
    catch (...)
    {
        try {
            auto addr = address( signer );
            return addr == address(fc::ecc::public_key( signature, hash ));
        }
        catch(...) {
           oaccount_record rec = blockchain_get_account( signer );
           if (!rec.valid())
              return false;

           // logic shamelessly copy-pasted from signed_block_header::validate_signee()
           // NB LHS of == operator is bts::blockchain::public_key_type and RHS is fc::ecc::public_key,
           //   the opposite order won't compile (look at operator== prototype in public_key_type class declaration)
           return rec->active_key() == fc::ecc::public_key(signature, hash);

        }
    }
}

vector<bts::blockchain::api_market_status> client_impl::blockchain_list_markets()const
{
   const vector<pair<asset_id_type, asset_id_type>> pairs = _chain_db->get_market_pairs();

   vector<bts::blockchain::api_market_status> statuses;
   statuses.reserve( pairs.size() );

   for( const auto pair : pairs )
   {
      const auto quote_record = _chain_db->get_asset_record( pair.first );
      const auto base_record = _chain_db->get_asset_record( pair.second );
      FC_ASSERT( quote_record.valid() && base_record.valid() );
      const auto status = _chain_db->get_market_status( quote_record->id, base_record->id );
      FC_ASSERT( status.valid() );

      api_market_status api_status( *status );
      price current_feed_price;
      price last_valid_feed_price;

      if( status->current_feed_price.valid() )
         current_feed_price = *status->current_feed_price;
      if( status->last_valid_feed_price.valid() )
         last_valid_feed_price = *status->last_valid_feed_price;

      api_status.current_feed_price = _chain_db->to_pretty_price_double( current_feed_price );
      api_status.last_valid_feed_price = _chain_db->to_pretty_price_double( last_valid_feed_price );

      statuses.push_back( api_status );
   }

   return statuses;
}

vector<bts::blockchain::market_transaction> client_impl::blockchain_list_market_transactions( uint32_t block_num )const
{
   return _chain_db->get_market_transactions( block_num );
}

bts::blockchain::api_market_status client_impl::blockchain_market_status( const std::string& quote,
                                                                          const std::string& base )const
{
   auto qrec = _chain_db->get_asset_record(quote);
   auto brec = _chain_db->get_asset_record(base);
   FC_ASSERT( qrec && brec );
   auto oresult = _chain_db->get_market_status( qrec->id, brec->id );
   FC_ASSERT( oresult, "The ${q}/${b} market has not yet been initialized.", ("q", quote)("b", base));

   api_market_status result(*oresult);
   price current_feed_price;
   price last_valid_feed_price;

   if( oresult->current_feed_price.valid() )
      current_feed_price = *oresult->current_feed_price;
   if( oresult->last_valid_feed_price.valid() )
      last_valid_feed_price = *oresult->last_valid_feed_price;

   result.current_feed_price = _chain_db->to_pretty_price_double( current_feed_price );
   result.last_valid_feed_price = _chain_db->to_pretty_price_double( last_valid_feed_price );
   return result;
}

bts::blockchain::asset client_impl::blockchain_unclaimed_genesis() const
{
   return _chain_db->unclaimed_genesis();
}

vector<burn_record> client_impl::blockchain_get_account_wall( const string& account )const
{
   return _chain_db->fetch_burn_records( account );
}

void client_impl::blockchain_broadcast_transaction(const signed_transaction& trx)
{
   auto collector = _chain_db->get_account_record(_config.relay_account_name);
   auto accept_fee = [this](const asset& fee) -> bool {
      auto feed_price = _chain_db->get_active_feed_price(fee.asset_id);
      if( !feed_price ) return false;

      //Forgive up to a 5% change in price from the last sync by the lightwallet
      asset required = asset(_config.light_relay_fee * .95) * *feed_price;
      return fee >= required;
   };

   if( collector && _config.light_relay_fee )
   {
      for( operation op : trx.operations )
         if( op.type == deposit_op_type )
         {
            deposit_operation deposit = op.as<deposit_operation>();
            ilog("Checking if deposit ${d} is to ${c}", ("d", deposit)("c", collector->active_address()));
            if( deposit.condition.owner() && *deposit.condition.owner() == collector->active_address() &&
                ( (deposit.condition.asset_id == 0 && deposit.amount >= _config.light_relay_fee) ||
                  accept_fee(asset(deposit.amount, deposit.condition.asset_id)) ) )
            {
               network_broadcast_transaction(trx);
               return;
            }
         }

      FC_THROW("Do I look like some kind of charity? You want your transactions sent, you pay like everyone else!");
   }
   network_broadcast_transaction(trx);
}


object_record client_impl::blockchain_get_object( const object_id_type& id )const
{
    auto oobj = _chain_db->get_object_record( id );
    FC_ASSERT( oobj.valid(), "No such object!" );
    return *oobj;
}

vector<edge_record> client_impl::blockchain_get_edges( const object_id_type& from,
                                                       const object_id_type& to,
                                                       const string& name )const
{ try {
    vector<edge_record> edges;
    if( name != "" )
    {
        auto oedge = _chain_db->get_edge( from, to, name );
        if( oedge.valid() )
            edges.push_back( oedge->as<edge_record>() );
    }
    else if( to != -1 )
    {
        auto name_map = _chain_db->get_edges( from, to );
        for( auto pair : name_map )
            edges.push_back( pair.second.as<edge_record>() );
    }
    else
    {
        auto from_map = _chain_db->get_edges( from );
        for( auto p1 : from_map )
            for( auto p2 : p1.second )
                edges.push_back( p2.second.as<edge_record>() );
    }
    return edges;
} FC_CAPTURE_AND_RETHROW( (from)(to)(name) ) }


} } } // namespace bts::client::detail
