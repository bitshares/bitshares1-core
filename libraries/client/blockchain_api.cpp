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

vector<block_record> client_impl::blockchain_list_blocks( const uint32_t max_block_num, const uint32_t limit )
{ try {
    FC_ASSERT( max_block_num > 0 );
    FC_ASSERT( limit > 0 );

    const uint32_t head_block_num = _chain_db->get_head_block_num();

    uint32_t start_block_num = head_block_num;
    if( max_block_num <= head_block_num )
    {
        start_block_num = max_block_num;
    }
    else
    {
        FC_ASSERT( -max_block_num <= head_block_num );
        start_block_num = head_block_num + max_block_num + 1;
    }

    const uint32_t count = std::min( limit, start_block_num );
    vector<block_record> records;
    records.reserve( count );

    for( uint32_t block_num = start_block_num; block_num > 0; --block_num )
    {
        oblock_record record = _chain_db->get_block_record( block_num );
        if( record.valid() ) records.push_back( std::move( *record ) );
        if( records.size() >= count ) break;
    }

    return records;
} FC_CAPTURE_AND_RETHROW( (max_block_num)(limit) ) }

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
vector<feed_entry> detail::client_impl::blockchain_get_feeds_for_asset( const string& asset )const
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

         const string price = _chain_db->to_pretty_price( feed.value, false );

         result_feeds.push_back({delegate->name, price, feed.last_update});
      }

      const auto omedian_price = _chain_db->get_active_feed_price( asset_id );
      if( omedian_price.valid() )
      {
          const string feed_price = _chain_db->to_pretty_price( *omedian_price, false );
          result_feeds.push_back( { "MARKET", "0", _chain_db->now(), _chain_db->get_asset_symbol(asset_id), feed_price } );
      }

      return result_feeds;
} FC_CAPTURE_AND_RETHROW( (asset) ) }

string detail::client_impl::blockchain_median_feed_price( const string& asset )const
{
   asset_id_type asset_id;
   if( !std::all_of( asset.begin(), asset.end(), ::isdigit) )
      asset_id = _chain_db->get_asset_id(asset);
   else
      asset_id = std::stoi( asset );

   const oprice feed_price = _chain_db->get_active_feed_price( asset_id );
   if( feed_price.valid() )
       return _chain_db->to_pretty_price( *feed_price, false );

   return "";
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
         const string price = _chain_db->to_pretty_price( raw_feed.value, false );
         const string asset_symbol = _chain_db->get_asset_symbol( raw_feed.index.quote_id );
         const oprice feed_price = _chain_db->get_active_feed_price( raw_feed.index.quote_id );
         optional<string> pretty_feed_price;
         if( feed_price.valid() )
             pretty_feed_price = _chain_db->to_pretty_price( *feed_price, false );

         result_feeds.push_back( feed_entry{ delegate_name, price, raw_feed.last_update, asset_symbol, pretty_feed_price } );
      }

      return result_feeds;
} FC_CAPTURE_AND_RETHROW( (delegate_name) ) }

pair<transaction_id_type, transaction_record> detail::client_impl::blockchain_get_transaction( const string& transaction_id_prefix,
                                                                                               bool exact )const
{ try {
   const auto id_prefix = variant( transaction_id_prefix ).as<transaction_id_type>();
   const otransaction_record record = _chain_db->get_transaction( id_prefix, exact );
   FC_ASSERT( record.valid(), "Transaction not found!" );
   return std::make_pair( record->trx.id(), *record );
} FC_CAPTURE_AND_RETHROW( (transaction_id_prefix)(exact) ) }

oblock_record detail::client_impl::blockchain_get_block( const string& block )const
{ try {
   try
   {
      ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
      if( block.size() == string( block_id_type() ).size() )
         return _chain_db->get_block_record( block_id_type( block ) );
      uint32_t num;
      try {
          fc::time_point_sec time(fc::time_point_sec::from_iso_string( block ));
          num = _chain_db->find_block_num( time );
      } catch( ... ) {
          num = std::stoi( block );
      }
      return _chain_db->get_block_record( num );
   }
   catch( ... )
   {
   }
   return oblock_record();
} FC_CAPTURE_AND_RETHROW( (block) ) }

unordered_map<balance_id_type, balance_record> detail::client_impl::blockchain_list_balances( const string& asset, uint32_t limit )const
{ try {
    FC_ASSERT( limit > 0 );
    const oasset_record asset_record = blockchain_get_asset( asset );

    unordered_map<balance_id_type, balance_record> records;

    const auto scan_balance = [ &records, &asset_record, limit ]( const balance_record& record )
    {
        if( records.size() >= limit )
            return;

        if( !asset_record.valid() || record.asset_id() == asset_record->id )
            records[ record.id() ] = record;
    };
    _chain_db->scan_balances( scan_balance );

    return records;
} FC_CAPTURE_AND_RETHROW( (asset)(limit) ) }

map<asset_id_type, share_type> detail::client_impl::blockchain_get_account_public_balance( const string& account_name )const
{ try {
    const oaccount_record account_record = _chain_db->get_account_record( account_name );
    FC_ASSERT( account_record.valid() );

    map<asset_id_type, share_type> result;

    const auto scan_key = [ & ]( const public_key_type& key )
    {
        const auto& balances = _chain_db->get_balances_for_key( key );
        for( const auto& item : balances )
        {
            const balance_record& balance = item.second;
            result[ balance.asset_id() ] += balance.balance;
        }
    };
    account_record->scan_public_keys( scan_key );

    return result;
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

unordered_map<balance_id_type, balance_record> detail::client_impl::blockchain_list_address_balances( const string& raw_addr,
                                                                                                      const time_point& after )const
{ try {
    address addr;
    try
    {
        addr = address( raw_addr );
    }
    catch( const fc::exception& )
    {
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
} FC_CAPTURE_AND_RETHROW( (raw_addr)(after) ) }

fc::variant_object detail::client_impl::blockchain_list_address_transactions( const string& raw_addr, uint32_t after_block )const
{ try {
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
} FC_CAPTURE_AND_RETHROW( (raw_addr)(after_block) ) }

unordered_map<balance_id_type, balance_record> detail::client_impl::blockchain_list_key_balances( const public_key_type& key )const
{ try {
    return _chain_db->get_balances_for_key( key );
} FC_CAPTURE_AND_RETHROW( (key) ) }

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

map<string, string> detail::client_impl::blockchain_list_feed_prices()const
{
    map<string, string> feed_prices;
    const auto scan_asset = [&]( const asset_record& record )
    {
        const auto median_price = _chain_db->get_active_feed_price( record.id );
        if( !median_price.valid() ) return;
        feed_prices.emplace( record.symbol, _chain_db->to_pretty_price( *median_price, false ) );
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

void client_impl::blockchain_graphene_snapshot( const string& filename, const string& whitelist_filename )const
{ try {
    set<string> whitelist;
    if( !whitelist_filename.empty() )
        whitelist = fc::json::from_file<std::set<string>>( whitelist_filename );

    _chain_db->graphene_snapshot( filename, whitelist );
} FC_CAPTURE_AND_RETHROW( (filename) ) }

void client_impl::blockchain_generate_issuance_map( const string& symbol, const string& filename )const
{ try {
    _chain_db->generate_issuance_map( symbol, fc::path( filename ) );
} FC_CAPTURE_AND_RETHROW( (filename) ) }

asset client_impl::blockchain_calculate_supply( const string& which_asset )const
{ try {
   asset_id_type asset_id;
   if( std::all_of( which_asset.begin(), which_asset.end(), ::isdigit ) )
      asset_id = std::stoi( which_asset );
   else
      asset_id = _chain_db->get_asset_id( which_asset );

   return asset( _chain_db->calculate_supplies().at( asset_id ), asset_id );
} FC_CAPTURE_AND_RETHROW( (which_asset) ) }

asset client_impl::blockchain_calculate_debt( const string& which_asset, bool include_interest )const
{ try {
   asset_id_type asset_id;
   if( std::all_of( which_asset.begin(), which_asset.end(), ::isdigit ) )
      asset_id = std::stoi( which_asset );
   else
      asset_id = _chain_db->get_asset_id( which_asset );

   return asset( _chain_db->calculate_debts( include_interest ).at( asset_id ), asset_id );
} FC_CAPTURE_AND_RETHROW( (which_asset)(include_interest) ) }

asset client_impl::blockchain_calculate_max_supply( uint8_t average_delegate_pay_rate )const
{ try {
   FC_ASSERT( average_delegate_pay_rate >= 0 );
   FC_ASSERT( average_delegate_pay_rate <= 100 );
   const share_type pay_per_block = BTS_MAX_DELEGATE_PAY_PER_BLOCK * average_delegate_pay_rate / 100;
   return asset( _chain_db->calculate_max_core_supply( pay_per_block ) );
} FC_CAPTURE_AND_RETHROW( (average_delegate_pay_rate) ) }

vector<market_order> client_impl::blockchain_market_list_bids( const string& quote_symbol, const string& base_symbol,
                                                               uint32_t limit )const
{
   return _chain_db->get_market_bids( quote_symbol, base_symbol, limit );
}

vector<market_order> client_impl::blockchain_market_list_asks( const string& quote_symbol, const string& base_symbol,
                                                               uint32_t limit )const
{
   return _chain_db->get_market_asks( quote_symbol, base_symbol, limit );
}

vector<market_order> client_impl::blockchain_market_list_shorts( const string& quote_symbol,
                                                                 uint32_t limit )const
{
   return _chain_db->get_market_shorts( quote_symbol, limit );
}
vector<market_order> client_impl::blockchain_market_list_covers( const string& quote_symbol, const string& base_symbol,
                                                                 uint32_t limit )const
{
   return _chain_db->get_market_covers( quote_symbol, base_symbol, limit );
}

share_type client_impl::blockchain_market_get_asset_collateral( const string& symbol )const
{
   return _chain_db->get_asset_collateral( symbol );
}

std::pair<vector<market_order>,vector<market_order>> client_impl::blockchain_market_order_book( const string& quote_symbol,
                                                                                                const string& base_symbol,
                                                                                                uint32_t limit )const
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

market_order client_impl::blockchain_get_market_order( const string& order_id_str )const
{
   fc::optional<market_order> order = _chain_db->get_market_order( order_id_type(order_id_str) );
   FC_ASSERT( order.valid(), "Order not found!" );
   return *order;
}

map<order_id_type, market_order> client_impl::blockchain_list_address_orders(
    const string& quote_symbol, const string& base_symbol,
    const string& account_address_str, uint32_t limit
)const { try {
  map<order_id_type, market_order> map;

  const oasset_record quote_record = _chain_db->get_asset_record( quote_symbol );
  const oasset_record base_record = _chain_db->get_asset_record( base_symbol );
  FC_ASSERT( quote_symbol.empty() || quote_record.valid() );
  FC_ASSERT( base_symbol.empty() || base_record.valid() );

  address account_address = address(account_address_str);

  const auto filter = [&]( const market_order& order ) -> bool
  {
      if( quote_record.valid() && order.market_index.order_price.quote_asset_id != quote_record->id )
          return false;

      if( base_record.valid() && order.market_index.order_price.base_asset_id != base_record->id )
          return false;

      if( account_address != order.get_owner() )
          return false;

      return true;
  };

  const auto orders = _chain_db->scan_market_orders( filter, limit );
  for( const auto& order : orders )
      map[ order.get_id() ] = order;

  return map;
} FC_CAPTURE_AND_RETHROW( (quote_symbol)(base_symbol)(limit)(account_address_str) ) }


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

vector<bts::blockchain::string_status_record> client_impl::blockchain_list_markets()const
{
   const vector<pair<asset_id_type, asset_id_type>> pairs = _chain_db->get_market_pairs();

   vector<bts::blockchain::string_status_record> statuses;
   statuses.reserve( pairs.size() );

   for( const auto pair : pairs )
   {
      const auto quote_record = _chain_db->get_asset_record( pair.first );
      const auto base_record = _chain_db->get_asset_record( pair.second );
      FC_ASSERT( quote_record.valid() && base_record.valid() );
      const auto status = _chain_db->get_status_record( status_index{ quote_record->id, base_record->id } );
      FC_ASSERT( status.valid() );

      string_status_record api_status;
      ( status_record& )api_status = *status;

      if( status->current_feed_price.valid() )
          api_status.current_feed_price = _chain_db->to_pretty_price( *status->current_feed_price, false );

      if( status->last_valid_feed_price.valid() )
          api_status.last_valid_feed_price = _chain_db->to_pretty_price( *status->last_valid_feed_price, false );

      statuses.push_back( api_status );
   }

   return statuses;
}

vector<bts::blockchain::market_transaction> client_impl::blockchain_list_market_transactions( uint32_t block_num )const
{
   return _chain_db->get_market_transactions( block_num );
}

bts::blockchain::string_status_record client_impl::blockchain_market_status( const std::string& quote,
                                                                          const std::string& base )const
{
   auto qrec = _chain_db->get_asset_record(quote);
   auto brec = _chain_db->get_asset_record(base);
   FC_ASSERT( qrec && brec );
   auto status = _chain_db->get_status_record( status_index{ qrec->id, brec->id } );
   FC_ASSERT( status, "The ${q}/${b} market has not yet been initialized.", ("q", quote)("b", base));

   string_status_record result;
   ( status_record& )result = *status;

   if( status->current_feed_price.valid() )
       result.current_feed_price = _chain_db->to_pretty_price( *status->current_feed_price, false );

   if( status->last_valid_feed_price.valid() )
       result.last_valid_feed_price = _chain_db->to_pretty_price( *status->last_valid_feed_price, false );

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

} } } // namespace bts::client::detail
