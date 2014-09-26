#pragma once

#include <bts/blockchain/transaction.hpp>
#include <bts/wallet/wallet_records.hpp>

#include <vector>
#include <map>

namespace bts { namespace wallet {
   namespace detail { class wallet_impl; }

   /**
    * @brief The transaction_builder struct simplifies the process of creating arbitrarily complex transactions.
    *
    * The builder creates a transaction and allows an arbitrary number of operations to be added to this transaction by
    * subsequent calls to the Builder Functions. The builder is capable of dealing with operations from multiple
    * accounts, and the various accounts' credits and debits are not mixed. When the builder finalizes, it balances the
    * books by crediting or charging each account with only that account's outstanding balance.
    *
    * The same owner key is used for all market operations belonging to a single account. Different accounts have
    * different market owner keys. If a new market operation is added to the transaction, and that operation belongs to
    * an account which also has order cancellations in this transaction, one of that account's cancellations is chosen
    * arbitrarily and the new market operations are owned by that cancellation's owner key. If no cancellations for
    * that account exist at the time of building in the new market operation, a new key pair is generated for the
    * owning account and used as the owner key for subsequent market operations under that account.
    *
    * The Builder Functions expect the wallet to be open and unlocked. Do not attempt to use a transaction_builder on a
    * wallet which is not unlocked. The functions make several other assumptions which must be validated before calling
    * the appropriate function. See the documentation for a given function to know exactly what assumptions it makes.
    * Calling a Builder Function with one of its assumptions being invalid yields undefined behavior.
    */
   struct transaction_builder {
      transaction_builder(detail::wallet_impl* wimpl)
          : _wimpl(wimpl)
      {}

      wallet_transaction_record transaction_record;
      std::unordered_set<blockchain::address> required_signatures;
      ///Map of <owning account address, asset ID> to that account's balance in that asset ID
      std::map<std::pair<blockchain::address, asset_id_type>, share_type> outstanding_balances;
      ///Map of account address to key owning that account's market transactions
      std::map<blockchain::address, public_key_type> order_keys;

      public_key_type order_key_for_account(const blockchain::address& account_address);

      /**
       * \defgroup<builders> Builder Functions
       * These functions each add one operation to the transaction. They all return
       * a reference to the builder, so they can be chained together in standard
       * builder syntax:
       * @code
       * builder.cancel_market_order(id1)
       *        .cancel_market_order(id2)
       *        .submit_bid(account, quantity, symbol, price, quote_symbol);
       * @endcode
       */
      /// @{
      /**
       * @brief Cancel a single order
       * @param order_id
       */
      transaction_builder& cancel_market_order(const order_id_type& order_id);
      /**
       * @brief Submit a bid order
       * @param from_account The account to place the bid
       * @param real_quantity The quantity to buy
       * @param quote_price The price to buy at
       *
       * The account is expected to be a receive account.
       * The quantity and price are expected to be positive values.
       * The price's quote and base IDs are expected to be registered with the blockchain.
       */
      transaction_builder& submit_bid(const wallet_account_record& from_account,
                                      const asset& real_quantity,
                                      const price& quote_price);
      /**
       * @brief Submit an ask order
       * @param from_account The account to place the ask
       * @param real_quantity The quantity to sell
       * @param quote_price The price to sell at
       *
       * The account is expected to be a receive account.
       * The quantity and price are expected to be positive values.
       * The price's quote and base IDs are expected to be registered with the blockchain.
       */
      transaction_builder& submit_ask(const wallet_account_record& from_account,
                                      const asset& cost,
                                      const price& quote_price);
      /// @}
   private:
      detail::wallet_impl* _wimpl;
      //Shorthand name for the signed_transaction
      signed_transaction& trx = transaction_record.trx;
   };
} } //namespace bts::wallet

FC_REFLECT( bts::wallet::transaction_builder, (transaction_record)(required_signatures)(outstanding_balances) )
