#include <bts/wallet/config.hpp>
#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/time.hpp>

#include <fc/crypto/sha256.hpp>

using namespace bts::wallet;
using namespace bts::wallet::detail;

void  transaction_builder::set_wallet_implementation(std::unique_ptr<bts::wallet::detail::wallet_impl>& wimpl)
{
    _wimpl = wimpl.get();
}

public_key_type transaction_builder::order_key_for_account(const address& account_address, const string& account_name)
{
   auto order_key = order_keys[account_address];
   if( order_key == public_key_type() )
   {
       order_key = _wimpl->get_new_public_key(account_name);
       order_keys[account_address] = order_key;
   }
   return order_key;
}

transaction_builder& transaction_builder::release_escrow( const account_record& payer,
                                                          const address& escrow_account,
                                                          const address& released_by_address,
                                                          share_type     amount_to_sender,
                                                          share_type     amount_to_receiver )
{ try {
   auto escrow_record = _wimpl->_blockchain->get_balance_record( escrow_account );
   FC_ASSERT( escrow_record.valid() );

   auto escrow_condition = escrow_record->condition.as<withdraw_with_escrow>();

   //deduct_balance( released_by_address, _wimpl->self->get_transaction_fee() );
   // TODO: this is a hack to bypass finalize() call...
   _wimpl->withdraw_to_transaction( _wimpl->self->get_transaction_fee(),
                                 payer.name,
                                 trx,
                                 required_signatures );
   // fetch balance record, assert that released_by_address is a party to the contract
   trx.release_escrow( escrow_account, released_by_address, amount_to_sender, amount_to_receiver );
   if( released_by_address == address() )
   {
      required_signatures.insert( escrow_condition.sender );
      required_signatures.insert( escrow_condition.receiver );
   }
   else
   {
      required_signatures.insert( released_by_address );
   }
   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + _wimpl->self->get_transaction_expiration();

   transaction_record.record_id = trx.id();
   transaction_record.created_time = blockchain::now();
   transaction_record.received_time = transaction_record.created_time;
   return *this;
} FC_CAPTURE_AND_RETHROW( (payer)(escrow_account)(released_by_address)(amount_to_sender)(amount_to_receiver) ) }

transaction_builder&transaction_builder::register_account(const string& name,
                                                          optional<variant> public_data,
                                                          public_key_type owner_key,
                                                          optional<public_key_type> active_key,
                                                          optional<uint8_t> delegate_pay,
                                                          optional<account_meta_info> meta_info,
                                                          optional<wallet_account_record> paying_account)
{ try {
   if( !public_data ) public_data = variant(fc::variant_object());
   if( !active_key ) active_key = owner_key;
   if( !delegate_pay ) delegate_pay = -1;

   if( paying_account ) deduct_balance(paying_account->owner_address(), asset());

   trx.register_account(name, *public_data, owner_key, *active_key, *delegate_pay, meta_info);

   ledger_entry entry;
   entry.from_account = paying_account->owner_key;
   entry.to_account = owner_key;
   entry.memo = "Register account: " + name;
   transaction_record.ledger_entries.push_back(entry);

   return *this;
} FC_CAPTURE_AND_RETHROW( (name)(public_data)(owner_key)(active_key)(delegate_pay)(paying_account) ) }

transaction_builder& transaction_builder::update_account_registration(const wallet_account_record& account,
                                                                      optional<variant> public_data,
                                                                      optional<public_key_type> active_key,
                                                                      optional<uint8_t> delegate_pay,
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
   deduct_balance(paying_account->owner_address(), asset());

   if( delegate_pay )
   {
      FC_ASSERT( !account.is_delegate() ||
                 *delegate_pay <= account.delegate_pay_rate(), "Pay rate can only be decreased!" );

      //If account is not a delegate but wants to become one OR account is a delegate changing his pay rate...
      if( (!account.is_delegate() && *delegate_pay <= 100) ||
           (account.is_delegate() && *delegate_pay != account.delegate_pay_rate()) )
      {
         if( !paying_account->is_my_account )
            FC_THROW_EXCEPTION( unknown_account, "Unknown paying account!", ("paying_account", paying_account) );

         asset fee(_wimpl->_blockchain->get_delegate_registration_fee( *delegate_pay ));
         if( paying_account->is_delegate() && paying_account->delegate_pay_balance() >= fee.amount )
         {
            //Withdraw into trx, but don't record it in outstanding_balances because it's a fee
            trx.withdraw_pay(paying_account->id, fee.amount);
            working_required_signatures.insert(paying_account->active_key());
         } else
            deduct_balance(paying_account->owner_address(), fee);

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
      if( _wimpl->_blockchain->get_account_record(*active_key).valid() ||
          _wimpl->_wallet_db.lookup_account(*active_key).valid() )
         FC_THROW_EXCEPTION( key_already_registered, "Key already belongs to another account!", ("new_public_key", active_key));

      ledger_entry entry;
      entry.from_account = paying_account->owner_key;
      entry.to_account = account.owner_key;
      entry.memo = "Update " + account.name + "'s active key";
      transaction_record.ledger_entries.push_back(entry);
   }

   trx.update_account(account.id, *delegate_pay, public_data, active_key);

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
                                                        const string& memo,
                                                        fc::optional<string> memo_sender)
{ try {
   if( recipient.is_retracted() )
       FC_CAPTURE_AND_THROW( account_retracted, (recipient) );

   if( amount.amount <= 0 )
      FC_THROW_EXCEPTION( invalid_asset_amount, "Cannot deposit a negative amount!" );

   // Don't automatically truncate memos as long as users still depend on them via deposit ops rather than mail
   if( memo.size() > BTS_BLOCKCHAIN_MAX_EXTENDED_MEMO_SIZE )
       FC_CAPTURE_AND_THROW( memo_too_long, (memo) );

   if( !memo_sender.valid() )
       memo_sender = payer.name;
   const owallet_account_record memo_account = _wimpl->_wallet_db.lookup_account( *memo_sender );
   FC_ASSERT( memo_account.valid() && memo_account->is_my_account );

   public_key_type memo_key = memo_account->owner_key;
   if( !_wimpl->_wallet_db.has_private_key( memo_key ) )
       memo_key = memo_account->active_key();
   FC_ASSERT( _wimpl->_wallet_db.has_private_key( memo_key ) );

   optional<public_key_type> titan_one_time_key;
   auto one_time_key = _wimpl->get_new_private_key(payer.name);
   titan_one_time_key = one_time_key.get_public_key();
   trx.deposit_to_account(recipient.active_key(), amount, _wimpl->self->get_private_key(memo_key), memo,
                          memo_key, one_time_key, from_memo, !recipient.is_public_account());

   deduct_balance(payer.owner_key, amount);

   ledger_entry entry;
   entry.from_account = payer.owner_key;
   entry.to_account = recipient.owner_key;
   entry.amount = amount;
   entry.memo = memo;
   if( *memo_sender != payer.name ) entry.memo_from_account = memo_account->owner_key;
   transaction_record.ledger_entries.push_back(std::move(entry));

   auto memo_signature = _wimpl->self->get_private_key(memo_key).sign_compact(fc::sha256::hash(memo.data(),
                                                                                                   memo.size()));
   notices.emplace_back(std::make_pair(mail::transaction_notice_message(string(memo),
                                                                        std::move(titan_one_time_key),
                                                                        std::move(memo_signature)),
                                       recipient.active_key()));

   return *this;
} FC_CAPTURE_AND_RETHROW( (recipient)(amount)(memo) ) }


transaction_builder& transaction_builder::deposit_asset_to_address(const wallet_account_record& payer,
                                                                   const address& to_addr,
                                                                   const asset& amount,
                                                                   const string& memo )
{ try {
   if( amount.amount <= 0 )
       FC_THROW_EXCEPTION( invalid_asset_amount, "Cannot deposit a negative amount!" );

   trx.deposit(to_addr, amount );
   deduct_balance(payer.owner_key, amount);

   ledger_entry entry;
   entry.from_account = payer.owner_key;
   entry.amount = amount;
   entry.memo = memo;
   transaction_record.ledger_entries.push_back(std::move(entry));

   return *this;
} FC_CAPTURE_AND_RETHROW( (to_addr)(amount)(memo) ) }

transaction_builder& transaction_builder::deposit_asset_to_multisig(
                                                    const asset& amount,
                                                    const string& from_name,
                                                    uint32_t m,
                                                    const vector<address>& addresses )
{ try {
   if( amount.amount <= 0 )
      FC_THROW_EXCEPTION( invalid_asset_amount, "Cannot deposit a negative amount!" );

   auto payer = _wimpl->_wallet_db.lookup_account( from_name );
   auto fee = _wimpl->self->get_transaction_fee();
   FC_ASSERT(payer.valid(), "No such account");
   multisig_meta_info info;
   info.required = m;
   info.owners = set<address>(addresses.begin(), addresses.end());
   trx.deposit_multisig(info, amount );

   deduct_balance(payer->owner_key, amount + fee);

   ledger_entry entry;
   entry.from_account = payer->owner_key;
   entry.amount = amount;
   transaction_record.ledger_entries.push_back(std::move(entry));

   return *this;
} FC_CAPTURE_AND_RETHROW( (from_name)(addresses)(amount) ) }

transaction_builder& transaction_builder::set_object(const string& payer_name,
                                                     const object_record& obj,
                                                     bool create )
{ try {
    auto payer = _wimpl->self->get_account( payer_name );
    deduct_balance( payer.owner_address(), asset() );
    int64_t id;
    if( create )
        id = 0;
    else
        id = obj.short_id();
    trx.set_object( obj );
    for( auto addr : _wimpl->_blockchain->get_object_condition( obj ).owners )
        required_signatures.insert( addr );

    return *this;
} FC_CAPTURE_AND_RETHROW( (payer_name)(obj)(create) ) }

transaction_builder& transaction_builder::set_edge(const string& payer_name,
                                                   const edge_record& edge )
{ try {
    ilog("@n building a set_edge transactoin");
    auto payer = _wimpl->self->get_account( payer_name );
    deduct_balance( payer.owner_address(), asset() );
    trx.set_edge( edge );
    auto obj = object_record( edge, edge_object, 0 );
    for( auto addr : _wimpl->_blockchain->get_object_condition( obj ).owners )
        required_signatures.insert( addr );
    return *this;
} FC_CAPTURE_AND_RETHROW( (payer_name)(edge) ) }


transaction_builder& transaction_builder::deposit_asset_with_escrow(const bts::wallet::wallet_account_record& payer,
                                                        const bts::blockchain::account_record& recipient,
                                                        const bts::blockchain::account_record& escrow_agent,
                                                        digest_type agreement,
                                                        const asset& amount,
                                                        const string& memo,
                                                        fc::optional<public_key_type> memo_sender)
{ try {
   if( recipient.is_retracted() )
       FC_CAPTURE_AND_THROW( account_retracted, (recipient) );

   if( escrow_agent.is_retracted() )
       FC_CAPTURE_AND_THROW( account_retracted, (escrow_agent) );

   if( amount.amount <= 0 )
      FC_THROW_EXCEPTION( invalid_asset_amount, "Cannot deposit a negative amount!" );

   // Don't automatically truncate memos as long as users still depend on them via deposit ops rather than mail
   if( memo.size() > BTS_BLOCKCHAIN_MAX_MEMO_SIZE )
       FC_CAPTURE_AND_THROW( memo_too_long, (memo) );

   if( !memo_sender.valid() )
       memo_sender = payer.active_key();

   optional<public_key_type> titan_one_time_key;
   if( recipient.is_public_account() )
   {
      // TODO: user public active receiver key...
   } else {
      auto one_time_key = _wimpl->get_new_private_key(payer.name);
      titan_one_time_key = one_time_key.get_public_key();
      auto receiver_key = trx.deposit_to_escrow(
                             recipient.active_key(),
                             escrow_agent.active_key(),
                             agreement,
                             amount,
                             _wimpl->self->get_private_key(*memo_sender),
                             memo,
                             *memo_sender,
                             one_time_key,
                             from_memo);

      key_data data;
      data.account_address = recipient.owner_address();
      data.public_key      = receiver_key;
      data.memo            = memo;
      _wimpl->_wallet_db.store_key( data );
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

transaction_builder& transaction_builder::cancel_market_order( const order_id_type& order_id )
{ try {
   const auto order = _wimpl->_blockchain->get_market_order( order_id );
   if( !order.valid() )
       FC_THROW_EXCEPTION( unknown_market_order, "Cannot find that market order!" );

   const auto owner_address = order->get_owner();
   const auto owner_key_record = _wimpl->_wallet_db.lookup_key( owner_address );
   if( !owner_key_record.valid() || !owner_key_record->has_private_key() )
       FC_THROW_EXCEPTION( private_key_not_found, "Cannot find the private key for that market order!" );

   const auto account_record = _wimpl->_wallet_db.lookup_account( owner_key_record->account_address );
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
      case relative_ask_order:
         trx.relative_ask( -balance, order->market_index.order_price, order->state.limit_price, owner_address );
         break;
      case relative_bid_order:
         trx.relative_bid( -balance, order->market_index.order_price, order->state.limit_price, owner_address );
         break;
      case short_order:
         trx.short_sell( -balance, order->market_index.order_price, owner_address );
         break;
      default:
         FC_THROW_EXCEPTION( invalid_cancel, "You cannot cancel this type of order!" );
         break;
   }

   //Credit this account the cancel proceeds
   credit_balance(account_record->owner_address(), balance);
   //Set order key for this account if not already set
   if( order_keys.find(account_record->owner_address()) == order_keys.end() )
      order_keys[account_record->owner_address()] = owner_key_record->public_key;

   auto entry = ledger_entry();
   entry.from_account = owner_key_record->public_key;
   entry.to_account = account_record->owner_key;
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
   asset cost;
   if( real_quantity.asset_id == quote_price.quote_asset_id )
       cost = real_quantity;
   else
   {
       cost = real_quantity * quote_price;
       FC_ASSERT(cost.asset_id == quote_price.quote_asset_id);
   }

   auto order_key = order_key_for_account(from_account.owner_address(), from_account.name);

   //Charge this account for the bid
   deduct_balance(from_account.owner_address(), cost);
   trx.bid(cost, quote_price, order_key);

   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + WALLET_DEFAULT_MARKET_TRANSACTION_EXPIRATION_SEC;

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


transaction_builder& transaction_builder::submit_relative_bid(const wallet_account_record& from_account,
                                                     const asset& sell_quantity,
                                                     const price& quote_price,
                                                     const optional<price>& limit )
{ try {
   validate_market(quote_price.quote_asset_id, quote_price.base_asset_id);

   auto order_key = order_key_for_account(from_account.owner_address(), from_account.name);

   //Charge this account for the bid
   deduct_balance(from_account.owner_address(), sell_quantity);
   trx.relative_bid(sell_quantity, quote_price, limit, order_key);

   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + WALLET_DEFAULT_MARKET_TRANSACTION_EXPIRATION_SEC;

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = order_key;
   entry.amount = sell_quantity;
   entry.memo = "relative buy " + _wimpl->_blockchain->get_asset_symbol(quote_price.base_asset_id) +
                " @ delta " + _wimpl->_blockchain->to_pretty_price(quote_price);

   transaction_record.is_market = true;
   transaction_record.ledger_entries.push_back(entry);

   required_signatures.insert(order_key);
   return *this;
} FC_CAPTURE_AND_RETHROW( (from_account.name)(sell_quantity)(quote_price) ) }

transaction_builder& transaction_builder::submit_ask(const wallet_account_record& from_account,
                                                     const asset& cost,
                                                     const price& quote_price)
{ try {
   validate_market(quote_price.quote_asset_id, quote_price.base_asset_id);
   FC_ASSERT(cost.asset_id == quote_price.base_asset_id);

   wdump((cost));

   auto order_key = order_key_for_account(from_account.owner_address(), from_account.name);

   //Charge this account for the ask
   deduct_balance(from_account.owner_address(), cost);
   trx.ask(cost, quote_price, order_key);

   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + WALLET_DEFAULT_MARKET_TRANSACTION_EXPIRATION_SEC;

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


transaction_builder& transaction_builder::submit_relative_ask(const wallet_account_record& from_account,
                                                     const asset& cost,
                                                     const price& quote_price,
                                                     const optional<price>& limit
                                                     )
{ try {
   validate_market(quote_price.quote_asset_id, quote_price.base_asset_id);
   FC_ASSERT(cost.asset_id == quote_price.base_asset_id);

   public_key_type order_key;

   order_key = order_key_for_account(from_account.owner_address(), from_account.name);

   //Charge this account for the ask
   deduct_balance(from_account.owner_address(), cost);
   trx.relative_ask(cost, quote_price, limit, order_key);

   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + WALLET_DEFAULT_MARKET_TRANSACTION_EXPIRATION_SEC;

   auto entry = ledger_entry();
   entry.from_account = from_account.owner_key;
   entry.to_account = order_key;
   entry.amount = cost;
   entry.memo = "relative sell " + _wimpl->_blockchain->get_asset_symbol(quote_price.base_asset_id) +
                " @ delta " + _wimpl->_blockchain->to_pretty_price(quote_price);

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
   FC_ASSERT( cost.asset_id == asset_id_type( 0 ), "You can only use the base asset as collateral!" );

   auto order_key = order_key_for_account(from_account.owner_address(), from_account.name);

   deduct_balance(from_account.owner_address(), cost);
   trx.short_sell(cost, interest_rate, order_key, price_limit);

   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + WALLET_DEFAULT_MARKET_TRANSACTION_EXPIRATION_SEC;

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

   if( accounts_with_covers.find(from_account.owner_address()) != accounts_with_covers.end() )
      FC_THROW_EXCEPTION( double_cover, "Cannot add a second cover for one account to transaction." );
   accounts_with_covers.insert(from_account.owner_address());

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
   if( order_keys.find(from_account.owner_address()) == order_keys.end() )
      order_keys[from_account.owner_address()] = owner_key_record->public_key;

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
   auto account_balances = _wimpl->self->get_spendable_account_balances( from_account.name );
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
      credit_balance(from_account.owner_address(), collateral);

      auto entry = ledger_entry();
      entry.from_account = owner_key_record->public_key;
      entry.to_account = from_account.owner_key;
      entry.amount = collateral;
      entry.memo = "cover proceeds";
      transaction_record.ledger_entries.push_back(entry);
   }

   //Commit the cover to transaction and charge the account.
   deduct_balance(from_account.owner_address(), cover_amount);
   trx.cover(cover_amount, order->market_index);

   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + WALLET_DEFAULT_MARKET_TRANSACTION_EXPIRATION_SEC;

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

transaction_builder& transaction_builder::update_signing_key( const string& authorizing_account_name,
                                                              const string& delegate_name,
                                                              const public_key_type& signing_key )
{ try {
    const owallet_account_record authorizing_account_record = _wimpl->_wallet_db.lookup_account( authorizing_account_name );
    if( !authorizing_account_record.valid() )
        FC_THROW_EXCEPTION( unknown_account, "Unknown authorizing account name!" );

    const oaccount_record delegate_record = _wimpl->_blockchain->get_account_record( delegate_name );
    if( !delegate_record.valid() )
        FC_THROW_EXCEPTION( unknown_account, "Unknown delegate account name!" );

    trx.update_signing_key( delegate_record->id, signing_key );
    deduct_balance( authorizing_account_record->owner_address(), asset() );

    ledger_entry entry;
    entry.from_account = authorizing_account_record->active_key();
    entry.to_account = delegate_record->owner_key;
    entry.memo = "update block signing key";

    transaction_record.ledger_entries.push_back( entry );

    required_signatures.insert( authorizing_account_record->active_key() );
    return *this;
} FC_CAPTURE_AND_RETHROW( (authorizing_account_name)(delegate_name)(signing_key) ) }

transaction_builder& transaction_builder::update_asset( const string& symbol,
                                                        const optional<string>& name,
                                                        const optional<string>& description,
                                                        const optional<variant>& public_data,
                                                        const optional<double>& maximum_share_supply,
                                                        const optional<uint64_t>& precision,
                                                        const share_type issuer_fee,
                                                        double market_fee,
                                                        uint32_t flags,
                                                        uint32_t issuer_perms,
                                                        const optional<account_id_type> issuer_account_id,
                                                        uint32_t required_sigs,
                                                        const vector<address>& authority
                                                        )
{ try {
    const oasset_record asset_record = _wimpl->_blockchain->get_asset_record( symbol );
    FC_ASSERT( asset_record.valid() );

    const oaccount_record issuer_account_record = _wimpl->_blockchain->get_account_record( asset_record->issuer_account_id );
    if( !issuer_account_record.valid() )
        FC_THROW_EXCEPTION( unknown_account, "Unknown issuer account id!" );

    account_id_type new_issuer_account_id;
    if( issuer_account_id.valid() )
        new_issuer_account_id = *issuer_account_id;
    else
        new_issuer_account_id = asset_record->issuer_account_id;

    uint16_t actual_market_fee = uint16_t(-1);
    if( market_fee >= 0 || market_fee <= BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE )
       actual_market_fee = BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE * market_fee;

    trx.update_asset_ext( asset_record->id, name, description, public_data, maximum_share_supply, precision,
                          issuer_fee, actual_market_fee, flags, issuer_perms, new_issuer_account_id, required_sigs, authority );
    deduct_balance( issuer_account_record->active_key(), asset() );

    ledger_entry entry;
    entry.from_account = issuer_account_record->active_key();
    entry.to_account = issuer_account_record->active_key();
    entry.memo = "update " + symbol + " asset";

    transaction_record.ledger_entries.push_back( entry );

    ilog("@n adding authority to required signatures: ${a}", ("a", asset_record->authority));
    for( auto owner : asset_record->authority.owners )
       required_signatures.insert( owner );
    return *this;
} FC_CAPTURE_AND_RETHROW( (symbol)(name)(description)(public_data)(maximum_share_supply)(precision) ) }

transaction_builder& transaction_builder::finalize( const bool pay_fee, const vote_strategy strategy )
{ try {
   FC_ASSERT( !trx.operations.empty(), "Cannot finalize empty transaction" );

   if( pay_fee )
       this->pay_fee();

   //outstanding_balance is pair<pair<account address, asset ID>, share_type>
   for( const auto& outstanding_balance : outstanding_balances )
   {
      asset balance(outstanding_balance.second, outstanding_balance.first.second);
      string account_name = _wimpl->_wallet_db.lookup_account(outstanding_balance.first.first)->name;

      if( balance.amount > 0 )
      {
          const address deposit_address = order_key_for_account(outstanding_balance.first.first, account_name);
          trx.deposit( deposit_address, balance );
      }
      else if( balance.amount < 0 )
      {
          _wimpl->withdraw_to_transaction(-balance, account_name, trx, required_signatures);
      }
   }

   if( trx.expiration == time_point_sec() )
       trx.expiration = blockchain::now() + _wimpl->self->get_transaction_expiration();

   _wimpl->set_delegate_slate( trx, strategy );

   transaction_record.record_id = trx.id();
   transaction_record.created_time = blockchain::now();
   transaction_record.received_time = transaction_record.created_time;

   return *this;
} FC_CAPTURE_AND_RETHROW( (trx) ) }

wallet_transaction_record& transaction_builder::sign()
{ try {
   const auto chain_id = _wimpl->_blockchain->get_chain_id();

   for( const auto& address : required_signatures )
   {
      //Ignore exceptions; this function operates on a best-effort basis, and doesn't actually have to succeed.
      try {
         ilog( "@n trying to sign for address ${a}", ("a",address));
         trx.sign(_wimpl->self->get_private_key(address), chain_id);
         ilog( "@n    and I succeeded");
      } catch( const fc::exception& e )
      {
         wlog( "unable to sign for address ${a}:\n${e}", ("a",address)("e",e.to_detail_string()) );
         ilog( "@n unable to sign for address ${a}:\n${e}", ("a",address)("e",e.to_detail_string()) );
      }
   }

   for( auto& notice : notices )
      notice.first.trx = trx;

   return transaction_record;
} FC_CAPTURE_AND_RETHROW() }

std::vector<bts::mail::message> transaction_builder::encrypted_notifications()
{
   vector<mail::message> messages;
   for( auto& notice : notices )
   {
      auto one_time_key = _wimpl->_wallet_db.generate_new_one_time_key(_wimpl->_wallet_password);
      messages.emplace_back(mail::message(notice.first).encrypt(one_time_key, notice.second));
   }
   return messages;
}
// First handles a margin position close and the collateral is returned.  Calls withdraw_fee() to
// to handle the more common cases.
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


transaction_builder& transaction_builder::withdraw_from_balance(const balance_id_type& from, const share_type amount)
{ try {
    // TODO ledger entries
    auto obalance = _wimpl->_blockchain->get_balance_record( from );
    if( obalance.valid() )
    {
        trx.withdraw( from, amount );
        for( const auto& owner : obalance->owners() )
            required_signatures.insert( owner );
    } else // We go ahead and try to use this balance ID as an owner
    {
        auto balances = _wimpl->_blockchain->get_balances_for_address( address(from) );
        FC_ASSERT( balances.size() > 0, "No balance with that ID or owner address!" );
        auto balance = balances.begin()->second;
        trx.withdraw( balance.id(), amount );
        for(const auto& owner : balance.owners() )
            required_signatures.insert( owner );
    }
    return *this;
} FC_CAPTURE_AND_RETHROW( (from)(amount) ) }

transaction_builder& transaction_builder::deposit_to_balance(const balance_id_type& to,
                                                             const asset& amount )
{
    // TODO ledger entries
    trx.deposit( to, amount );
    return *this;
}

transaction_builder& transaction_builder::asset_authorize_key( const string& symbol,
                                                               const address& owner,
                                                               object_id_type meta )
{ try {
   const oasset_record asset_record = _wimpl->_blockchain->get_asset_record( symbol );
   FC_ASSERT( asset_record.valid() );
   trx.authorize_key( asset_record->id, owner, meta );
   return *this;
} FC_CAPTURE_AND_RETHROW( (symbol)(owner)(meta) ) }

//Most common case where the fee gets paid
//Called when pay_fee doesn't find a positive balance in the trx to pay the fee with
bool transaction_builder::withdraw_fee()
{
   const auto balances = _wimpl->self->get_spendable_account_balances();

   //Shake 'em down
   for( const auto& item : outstanding_balances )
   {
      const address& bag_holder = item.first.first;

      //Got any lunch money?
      const owallet_account_record account_rec = _wimpl->_wallet_db.lookup_account(bag_holder);
      if( !account_rec || balances.count( account_rec->name ) == 0 )
         continue;

      //Well how much?
      const map<asset_id_type, share_type>& account_balances = balances.at( account_rec->name );
      for( const auto& balance_item : account_balances )
      {
          const asset balance( balance_item.second, balance_item.first );
          const asset fee = _wimpl->self->get_transaction_fee( balance.asset_id );
          if( fee.asset_id != balance.asset_id || fee > balance )
             //Forget this cheapskate
             continue;

          //Got some money, do ya? Not anymore!
          deduct_balance(bag_holder, fee);
          transaction_record.fee = fee;
          return true;
      }
   }
   return false;
}
