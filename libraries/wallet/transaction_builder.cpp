#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

#include <bts/blockchain/time.hpp>

using namespace bts::wallet;
using namespace bts::wallet::detail;

public_key_type transaction_builder::order_key_for_account(const address& account_address)
{
   auto order_key = order_keys[account_address];
   if( order_key == public_key_type() )
      order_key = _wimpl->_wallet_db.new_private_key(_wimpl->_wallet_password, account_address).get_public_key();
   return order_key;
}

transaction_builder& transaction_builder::cancel_market_order(const order_id_type& order_id)
{ try {
   const auto order = _wimpl->_blockchain->get_market_order( order_id );
   if( !order.valid() )
       FC_THROW_EXCEPTION( unknown_market_order, "Cannot find that market order!" );

   const auto owner_address = order->get_owner();
   const auto owner_key_record = _wimpl->_wallet_db.lookup_key( owner_address );
   if( !owner_key_record.valid() || !owner_key_record->has_private_key() )
       FC_THROW_EXCEPTION( private_key_not_found, "Cannot find the private key for that market order!" );

   const auto account_key_record = _wimpl->_wallet_db.lookup_key( owner_key_record->account_address );
   FC_ASSERT( account_key_record.valid() && account_key_record->has_private_key() );

   const auto account_record = _wimpl->_wallet_db.lookup_account( account_key_record->public_key );
   FC_ASSERT( account_record.valid() );

   asset balance = order->get_balance();
   if( balance.amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount, (order) );

   switch( order_type_enum( order->type ) )
   {
      case ask_order:
         trx.ask( -balance, order->market_index.order_price, owner_address );
         break;
      case bid_order:
         trx.bid( -balance, order->market_index.order_price, owner_address );
         break;
      case short_order:
         trx.short_sell( -balance, order->market_index.order_price, owner_address );
         break;
      default:
         FC_THROW_EXCEPTION( invalid_cancel, "You cannot cancel this type of order!" );
         break;
   }

   //Credit this account the cancel proceeds
   outstanding_balances[std::make_pair(account_record->account_address, balance.asset_id)] += balance.amount;
   //Set order key for this account if not already set
   if( order_keys.find(account_record->account_address) == order_keys.end() )
      order_keys[account_record->account_address] = owner_key_record->public_key;

   auto entry = ledger_entry();
   entry.from_account = owner_key_record->public_key;
   entry.to_account = account_key_record->public_key;
   entry.amount = balance;
   entry.memo = "cancel " + order->get_small_id();

   transaction_record.is_market = true;
   transaction_record.ledger_entries.push_back( entry );

   required_signatures.insert( owner_address );
   return *this;
} FC_CAPTURE_AND_RETHROW( (order_id) ) }

transaction_builder& transaction_builder::submit_bid(const wallet_account_record& from_account,
                                                     const asset& real_quantity,
                                                     const price& quote_price)
{ try {
   validate_market(quote_price.quote_asset_id, quote_price.base_asset_id);

   asset cost = real_quantity * quote_price;
   FC_ASSERT(cost.asset_id == quote_price.quote_asset_id);

   auto order_key = order_key_for_account(from_account.account_address);

   //Charge this account for the bid
   outstanding_balances[std::make_pair(from_account.account_address, cost.asset_id)] -= cost.amount;
   trx.bid(cost, quote_price, order_key);

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = order_key;
   entry.amount = cost;
   entry.memo = "buy " + _wimpl->_blockchain->get_asset_symbol(quote_price.base_asset_id) +
                " @ " + _wimpl->_blockchain->to_pretty_price(quote_price);

   transaction_record.is_market = true;
   transaction_record.ledger_entries.push_back(entry);

   required_signatures.insert(order_key);
   return *this;
} FC_CAPTURE_AND_RETHROW( (from_account.name)(real_quantity)(quote_price) ) }

transaction_builder& transaction_builder::submit_ask(const wallet_account_record& from_account,
                                                     const asset& cost,
                                                     const price& quote_price)
{ try {
   validate_market(quote_price.quote_asset_id, quote_price.base_asset_id);
   FC_ASSERT(cost.asset_id == quote_price.base_asset_id);

   auto order_key = order_key_for_account(from_account.account_address);

   //Charge this account for the ask
   outstanding_balances[std::make_pair(from_account.account_address, cost.asset_id)] -= cost.amount;
   trx.ask(cost, quote_price, order_key);

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = order_key;
   entry.amount = cost;
   entry.memo = "sell " + _wimpl->_blockchain->get_asset_symbol(quote_price.base_asset_id) +
                " @ " + _wimpl->_blockchain->to_pretty_price(quote_price);

   transaction_record.is_market = true;
   transaction_record.ledger_entries.push_back(entry);

   required_signatures.insert(order_key);
   return *this;
} FC_CAPTURE_AND_RETHROW( (from_account.name)(cost)(quote_price) ) }

transaction_builder& transaction_builder::submit_short(const wallet_account_record& from_account,
                                                       const asset& short_collateral_amount,
                                                       const price& interest_rate, // percent apr
                                                       const oprice& price_limit)
{ try {
   validate_market(interest_rate.quote_asset_id, interest_rate.base_asset_id);
   FC_ASSERT(!price_limit ||
             interest_rate.quote_asset_id == price_limit->quote_asset_id &&
             interest_rate.base_asset_id == price_limit->base_asset_id,
             "Interest rate ${rate} and price limit ${limit} do not have compatible units.",
             ("rate", interest_rate)("limit", price_limit));

   asset cost = short_collateral_amount;

   auto order_key = order_key_for_account(from_account.account_address);

   outstanding_balances[std::make_pair(from_account.account_address, cost.asset_id)] -= cost.amount;
   trx.short_sell(cost, interest_rate, order_key, price_limit);

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = order_key;
   entry.amount = cost;
   entry.memo = "short " + _wimpl->_blockchain->get_asset_symbol(interest_rate.quote_asset_id) +
                " @ " + interest_rate.ratio_string() + "% APR";

   transaction_record.is_market = true;
   transaction_record.ledger_entries.push_back(entry);

   required_signatures.insert(order_key);
   return *this;
} FC_CAPTURE_AND_RETHROW( (from_account.name)(short_collateral_amount)(interest_rate)(price_limit) ) }

transaction_builder& transaction_builder::submit_cover(const wallet_account_record& from_account,
                                                       asset cover_amount,
                                                       const order_id_type& order_id)
{ try {
   omarket_order order = _wimpl->_blockchain->get_market_order( order_id );
   if( !order.valid() )
       FC_THROW_EXCEPTION( unknown_market_order, "Cannot find that market order!" );

   const auto owner_address = order->get_owner();
   const auto owner_key_record = _wimpl->_wallet_db.lookup_key( owner_address );
   if( !owner_key_record.valid() || !owner_key_record->has_private_key() )
       FC_THROW_EXCEPTION( private_key_not_found, "Cannot find the private key for that market order!" );

   const auto account_key_record = _wimpl->_wallet_db.lookup_key( owner_key_record->account_address );
   FC_ASSERT( account_key_record.valid() && account_key_record->has_private_key() );

   const auto account_record = _wimpl->_wallet_db.lookup_account( account_key_record->public_key );
   FC_ASSERT( account_record.valid() );
   FC_ASSERT( account_record->name == from_account.name,
              "Refusing to cover order belonging to ${owner} when ${from} requested the cover!",
              ("owner", account_record->name)("from", from_account.name) );

   if( accounts_with_covers.find(from_account.account_address) != accounts_with_covers.end() )
      FC_THROW_EXCEPTION( double_cover, "Cannot add a second cover for one account to transaction." );
   accounts_with_covers.insert(from_account.account_address);

   //Check other pending transactions for other covers belonging to this account
   const auto pending = _wimpl->_blockchain->get_pending_transactions();
   for( const auto& eval : pending )
   {
       for( const auto& op : eval->trx.operations )
       {
           if( operation_type_enum( op.type ) != cover_op_type ) continue;
           const auto cover_op = op.as<cover_operation>();
           if( cover_op.cover_index.owner == owner_address )
               FC_THROW_EXCEPTION( double_cover, "You cannot cover a short twice in the same block!" );
       }
   }

   //Set order key for this account if not already set
   if( order_keys.find(from_account.account_address) == order_keys.end() )
      order_keys[from_account.account_address] = owner_key_record->public_key;

   asset order_balance = order->get_balance();
   if( order_balance.amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount, (order) );
   FC_ASSERT( order_balance.asset_id == cover_amount.asset_id,
              "Asset types of cover amount ${c} and short position ${s} do not match.",
              ("c", cover_amount.asset_id)("s", order_balance.asset_id) );

   //Don't over-cover the short position
   if( cover_amount > order_balance || cover_amount.amount == 0 )
       cover_amount = order_balance;

   order_balance -= cover_amount;
   if( order_balance.amount == 0 )
   {
      //If cover consumes short position, recover the collateral
      asset collateral(*order->collateral);
      outstanding_balances[std::make_pair(from_account.account_address, collateral.asset_id)] += collateral.amount;

      auto entry = ledger_entry();
      entry.from_account = owner_key_record->public_key;
      entry.to_account = from_account.owner_key;
      entry.amount = collateral;
      entry.memo = "cover proceeds";
      transaction_record.ledger_entries.push_back(entry);
   }

   //Commit the cover to transaction and charge the account.
   outstanding_balances[std::make_pair(from_account.account_address, cover_amount.asset_id)] -= cover_amount.amount;
   trx.cover(cover_amount, order->market_index);

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = owner_key_record->public_key;
   entry.amount = cover_amount;
   entry.memo = "payoff debt";
   transaction_record.ledger_entries.push_back(entry);

   required_signatures.insert(owner_key_record->public_key);
   return *this;
} FC_CAPTURE_AND_RETHROW( (from_account.name)(cover_amount)(order_id) ) }

transaction_builder& transaction_builder::finalize()
{ try {
   FC_ASSERT( !trx.operations.empty(), "Cannot finalize empty transaction" );

   auto slate = _wimpl->self->select_delegate_vote(vote_recommended);
   auto slate_id = slate.id();
   if( slate.supported_delegates.size() > 0 && !_wimpl->_blockchain->get_delegate_slate(slate_id) )
      trx.define_delegate_slate(slate);
   else
      slate_id = 0;

   pay_fees();

   //outstanding_balance is pair<pair<account address, asset ID>, share_type>
   for( auto outstanding_balance : outstanding_balances )
   {
      asset balance(outstanding_balance.second, outstanding_balance.first.second);
      address deposit_address = order_key_for_account(outstanding_balance.first.first);
      string account_name = _wimpl->_wallet_db.lookup_account(outstanding_balance.first.first)->name;

      if( balance.amount == 0 ) continue;
      else if( balance.amount > 0 ) trx.deposit(deposit_address, balance, slate_id);
      else _wimpl->withdraw_to_transaction(-balance, account_name, trx, required_signatures);
   }

   return *this;
} FC_CAPTURE_AND_RETHROW( (trx) ) }

wallet_transaction_record& transaction_builder::sign()
{
   auto chain_id = _wimpl->_blockchain->chain_id();
   trx.expiration = blockchain::now() + _wimpl->self->get_transaction_expiration();

   for( auto address : required_signatures )
   {
      //Ignore exceptions; this function operates on a best-effort basis, and doesn't actually have to succeed.
      try {
         trx.sign(_wimpl->self->get_private_key(address), chain_id);
      } catch( ... ) {}
   }

   _wimpl->self->cache_transaction(trx, transaction_record);
   return transaction_record;
}

void transaction_builder::pay_fees()
{ try {
   auto available_balances = all_positive_balances();
   asset required_fee(0, -1);

   //Choose an asset capable of paying the fee
   for( auto itr = available_balances.begin(); itr != available_balances.end(); ++itr )
      if( itr->second >= _wimpl->self->get_transaction_fee(itr->first).amount )
      {
         required_fee = _wimpl->self->get_transaction_fee(itr->first);
         transaction_record.fee = required_fee;
         break;
      }

   if( required_fee.asset_id != -1 )
   {
      transaction_record.fee = required_fee;
      //outstanding_balance is pair<pair<account address, asset ID>, share_type>
      for( auto& outstanding_balance : outstanding_balances )
      {
         if( outstanding_balance.first.second != required_fee.asset_id )
            //Not the right asset type
            continue;
         if( required_fee.amount > outstanding_balance.second )
         {
            //Balance can't cover the fee. Eat it and look for more.
            required_fee.amount -= outstanding_balance.second;
            outstanding_balance.second = 0;
            continue;
         }
         //We have enough to pay the fee. Pay it and let's get out of here.
         outstanding_balance.second -= required_fee.amount;
         return;
      }
   }
   else if( withdraw_fee() ) return;

   FC_THROW( "Unable to pay fee; no remaining balances can cover it and no account can pay it." );
} FC_RETHROW_EXCEPTIONS( warn, "All balances: ${bals}", ("bals", outstanding_balances) ) }

bool transaction_builder::withdraw_fee()
{
   //Shake down every owner with a balance until you find one who can pay a fee
   std::unordered_set<blockchain::address> checked_accounts;
   //At this point, we'll require XTS.
   asset final_fee= _wimpl->self->get_transaction_fee();
   address bag_holder;

   for( auto itr = outstanding_balances.begin(); itr != outstanding_balances.end(); ++itr )
   {
      address potential_bag_holder = itr->first.first;

      //Have we already vetted this potential bag holder?
      if( checked_accounts.find(potential_bag_holder) != checked_accounts.end() ) continue;
      checked_accounts.insert(potential_bag_holder);

      //Am I allowed to take money from this potential bag holder?
      auto account_rec = _wimpl->_wallet_db.lookup_account(potential_bag_holder);
      if( !account_rec || !account_rec->is_my_account ) continue;

      //Does this potential bag holder have any money I can take?
      account_balance_summary_type balances = _wimpl->self->get_account_balances(account_rec->name);
      if( balances.empty() ) continue;

      //Does this potential bag holder have enough XTS?
      auto balance_map = balances.begin()->second;
      if( balance_map.find(0) == balance_map.end() || balance_map[0] < final_fee.amount ) continue;

      //Let this potential bag holder be THE bag holder.
      bag_holder = potential_bag_holder;
      break;
   }

   if( bag_holder != address() )
   {
      outstanding_balances[std::make_pair(bag_holder, final_fee.asset_id)] -= final_fee.amount;
      transaction_record.fee = final_fee;
      return true;
   }
   return false;
}
