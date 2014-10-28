#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/time.hpp>

#include <bts/cli/pretty.hpp>

#include <fc/crypto/sha256.hpp>

using namespace bts::wallet;
using namespace bts::wallet::detail;

public_key_type transaction_builder::order_key_for_account(const address& account_address)
{
   auto order_key = order_keys[account_address];
   if( order_key == public_key_type() )
      order_key = _wimpl->_wallet_db.new_private_key(_wimpl->_wallet_password, account_address).get_public_key();
   return order_key;
}

transaction_builder& transaction_builder::update_account_registration(const wallet_account_record& account,
                                                                      optional<variant> public_data,
                                                                      optional<bts::blockchain::private_key_type> active_key,
                                                                      optional<share_type> delegate_pay,
                                                                      optional<wallet_account_record> paying_account)
{
   if( account.registration_date == fc::time_point_sec() )
      FC_THROW_EXCEPTION( unknown_account, "Account is not registered! Cannot update registration." );
   FC_ASSERT( public_data || active_key || delegate_pay, "Nothing to do!" );

   //Check at the beginning that we actually have the keys required to sign this thing.
   //Work on a copy so if we fail later, we haven't changed required_signatures.
   auto working_required_signatures = required_signatures;
   _wimpl->authorize_update(working_required_signatures, account, active_key.valid());

   if( !paying_account )
      paying_account = account;

   //Add paying_account to the transactions set of balance holders; he may be liable for the transaction fee.
   deduct_balance(paying_account->account_address, asset());

   if( delegate_pay )
   {
      FC_ASSERT( !account.is_delegate() ||
                 *delegate_pay <= account.delegate_pay_rate(), "Pay rate can only be decreased!" );

      //If account is not a delegate but wants to become one OR account is a delegate changing his pay rate...
      if( (!account.is_delegate() && *delegate_pay >= 0) ||
           (account.is_delegate() && *delegate_pay != account.delegate_pay_rate()) )
      {
         if( !paying_account->is_my_account )
            FC_THROW_EXCEPTION( unknown_account, "Unknown paying account!", ("paying_account", paying_account) );

         asset fee(_wimpl->_blockchain->get_delegate_registration_fee(*delegate_pay));
         if( paying_account->is_delegate() && paying_account->delegate_pay_balance() >= fee.amount )
         {
            //Withdraw into trx, but don't record it in outstanding_balances because it's a fee
            trx.withdraw_pay(paying_account->id, fee.amount);
            working_required_signatures.insert(paying_account->active_key());
         } else
            deduct_balance(paying_account->account_address, fee);

         ledger_entry entry;
         entry.from_account = paying_account->owner_key;
         entry.to_account = account.owner_key;
         entry.amount = fee;
         if( account.is_delegate() )
            entry.memo = "Fee to update " + account.name + "'s delegate pay";
         else
            entry.memo = "Fee to promote " + account.name + " to a delegate";
         transaction_record.ledger_entries.push_back(entry);
      }
   } else delegate_pay = account.delegate_pay_rate();

   fc::optional<public_key_type> active_public_key;
   if( active_key )
   {
      active_public_key = active_key->get_public_key();
      if( _wimpl->_blockchain->get_account_record(*active_public_key).valid() ||
          _wimpl->_wallet_db.lookup_account(*active_public_key).valid() )
         FC_THROW_EXCEPTION( key_already_registered, "Key already belongs to another account!", ("new_public_key", active_public_key));

      key_data new_key;
      new_key.encrypt_private_key(_wimpl->_wallet_password, *active_key);
      new_key.account_address = account.account_address;
      _wimpl->_wallet_db.store_key(new_key);

      ledger_entry entry;
      entry.from_account = paying_account->owner_key;
      entry.to_account = account.owner_key;
      entry.memo = "Update " + account.name + "'s active key";
      transaction_record.ledger_entries.push_back(entry);
   }

   trx.update_account(account.id, *delegate_pay, public_data, active_public_key);

   if( public_data )
   {
      ledger_entry entry;
      entry.from_account = paying_account->owner_key;
      entry.to_account = account.owner_key;
      entry.memo = "Update " + account.name + "'s public data";
      transaction_record.ledger_entries.push_back(entry);
   }

   required_signatures = working_required_signatures;
   return *this;
}

transaction_builder& transaction_builder::deposit_asset(const bts::wallet::wallet_account_record& payer,
                                                        const bts::blockchain::account_record& recipient,
                                                        const asset& amount,
                                                        const string& memo, vote_selection_method vote_method,
                                                        fc::optional<public_key_type> memo_sender)
{ try {
   if( amount.amount <= 0 )
      FC_THROW_EXCEPTION( invalid_asset_amount, "Cannot deposit a negative amount!" );
   optional<public_key_type> titan_one_time_key;

   if( !memo_sender.valid() )
       memo_sender = payer.active_key();

   if( recipient.is_public_account() )
   {
      trx.deposit(recipient.active_key(), amount, _wimpl->select_slate(trx, amount.asset_id, vote_method));
   } else {
      auto one_time_key = _wimpl->create_one_time_key();
      titan_one_time_key = one_time_key.get_public_key();
      trx.deposit_to_account(recipient.active_key(),
                             amount,
                             _wimpl->self->get_private_key(*memo_sender),
                             cli::pretty_shorten(memo, BTS_BLOCKCHAIN_MAX_MEMO_SIZE),
                             _wimpl->select_slate(trx, amount.asset_id, vote_method),
                             *memo_sender,
                             one_time_key,
                             from_memo);
   }

   deduct_balance(payer.owner_key, amount);

   ledger_entry entry;
   entry.from_account = payer.owner_key;
   entry.to_account = recipient.owner_key;
   entry.amount = amount;
   entry.memo = memo;
   if( *memo_sender != payer.active_key() )
      entry.memo_from_account = *memo_sender;
   transaction_record.ledger_entries.push_back(std::move(entry));

   auto memo_signature = _wimpl->self->get_private_key(*memo_sender).sign_compact(fc::sha256::hash(memo.data(),
                                                                                                   memo.size()));
   notices.emplace_back(std::make_pair(mail::transaction_notice_message(string(memo),
                                                                        std::move(titan_one_time_key),
                                                                        std::move(memo_signature)),
                                       recipient.active_key()));

   return *this;
} FC_CAPTURE_AND_RETHROW( (recipient)(amount)(memo) ) }

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
   credit_balance(account_record->account_address, balance);
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
   deduct_balance(from_account.account_address, cost);
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
   deduct_balance(from_account.account_address, cost);
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

   deduct_balance(from_account.account_address, cost);
   trx.short_sell(cost, interest_rate, order_key, price_limit);

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = order_key;
   entry.amount = cost;
   auto pretty_rate = interest_rate;
   pretty_rate.ratio *= 100;
   entry.memo = "short " + _wimpl->_blockchain->get_asset_symbol(interest_rate.quote_asset_id) +
                " @ " + pretty_rate.ratio_string() + "% APR";

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
   //Add interest to the balance
   auto age_at_transaction_expiration = _wimpl->_blockchain->now() + _wimpl->self->get_transaction_expiration() -
                     (*order->expiration - BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC);
   order_balance += blockchain::detail::market_engine
           ::get_interest_owed(order_balance, *order->interest_rate, age_at_transaction_expiration.to_seconds());

   //What's the account's actual balance? We can't pay more than that.
   auto account_balances = _wimpl->self->get_account_balances(from_account.name);
   FC_ASSERT( !account_balances.empty(), "Account has no balances! Cannot cover." );
   const auto& balances = account_balances.begin()->second;
   FC_ASSERT( balances.find(order_balance.asset_id) != balances.end(),
              "Account has no balance in asset to cover!" );
   asset max_cover_amount(balances.find(order_balance.asset_id)->second, order_balance.asset_id);

   //Don't over-cover the short position
   if( cover_amount > order_balance || cover_amount.amount == 0 )
       cover_amount = order_balance;

   FC_ASSERT( cover_amount <= max_cover_amount, "Cannot cover by ${cover_amount} as account only has ${balance}",
              ("cover_amount", cover_amount)("balance", max_cover_amount) );

   order_balance -= cover_amount;
   if( order_balance.amount == 0 )
   {
      //If cover consumes short position, recover the collateral
      asset collateral(*order->collateral);
      credit_balance(from_account.account_address, collateral);

      auto entry = ledger_entry();
      entry.from_account = owner_key_record->public_key;
      entry.to_account = from_account.owner_key;
      entry.amount = collateral;
      entry.memo = "cover proceeds";
      transaction_record.ledger_entries.push_back(entry);
   }

   //Commit the cover to transaction and charge the account.
   deduct_balance(from_account.account_address, cover_amount);
   trx.cover(cover_amount, order->market_index);

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = owner_key_record->public_key;
   entry.amount = cover_amount;
   entry.memo = "payoff debt";

   transaction_record.is_market = true;
   transaction_record.ledger_entries.push_back(entry);

   required_signatures.insert(owner_key_record->public_key);
   return *this;
} FC_CAPTURE_AND_RETHROW( (from_account.name)(cover_amount)(order_id) ) }

transaction_builder& transaction_builder::finalize()
{ try {
   FC_ASSERT( !trx.operations.empty(), "Cannot finalize empty transaction" );

   auto slate = _wimpl->select_delegate_vote(vote_recommended);
   auto slate_id = slate.id();
   if( slate.supported_delegates.size() > 0 && !_wimpl->_blockchain->get_delegate_slate(slate_id) )
      trx.define_delegate_slate(slate);
   else
      slate_id = 0;

   pay_fee();

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

   for( auto& notice : notices )
      notice.first.trx = trx;

   _wimpl->cache_transaction(trx, transaction_record);
   return transaction_record;
}

std::vector<bts::mail::message> transaction_builder::encrypted_notifications()
{
   vector<mail::message> messages;
   for( auto& notice : notices )
      messages.emplace_back(mail::message(notice.first).encrypt(_wimpl->create_one_time_key(), notice.second));
   return messages;
}

void transaction_builder::pay_fee()
{ try {
   auto available_balances = all_positive_balances();
   asset required_fee(0, -1);

   //Choose an asset capable of paying the fee
   for( auto itr = available_balances.begin(); itr != available_balances.end(); ++itr )
      if( _wimpl->self->asset_can_pay_fee(itr->first) &&
          itr->second >= _wimpl->self->get_transaction_fee(itr->first).amount )
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
      deduct_balance(bag_holder, final_fee);
      transaction_record.fee = final_fee;
      return true;
   }
   return false;
}
