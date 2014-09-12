#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/blockchain/transaction.hpp>

#include <fc/io/raw_variant.hpp>

namespace bts { namespace blockchain {

   digest_type transaction::digest( const digest_type& chain_id )const
   {
      fc::sha256::encoder enc;
      fc::raw::pack(enc,*this);
      fc::raw::pack(enc,chain_id);
      return enc.result();
   }

   size_t signed_transaction::data_size()const
   {
      fc::datastream<size_t> ds;
      fc::raw::pack(ds,*this);
      return ds.tellp();
   }

   transaction_id_type signed_transaction::id()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack(enc,*this);
      return fc::ripemd160::hash( enc.result() );
   }

   void signed_transaction::sign( const fc::ecc::private_key& signer, const digest_type& chain_id )
   {
      signatures.push_back( signer.sign_compact( digest(chain_id) ) );
   }

   void transaction::define_delegate_slate( delegate_slate s )
   {
      FC_ASSERT( s.supported_delegates.size() > 0 );
      operations.emplace_back( define_delegate_slate_operation( std::move(s) ) );
   }

   void transaction::burn( const asset& quantity, account_id_type for_or_against )
   {
   }

   void transaction::bid( const asset& quantity,
                          const price& price_per_unit,
                          const address& owner )
   {
      bid_operation op;
      op.amount = quantity.amount;
      op.bid_index.order_price = price_per_unit;
      op.bid_index.owner = owner;

      operations.emplace_back(op);
   }

   void transaction::ask( const asset& quantity,
                          const price& price_per_unit,
                          const address& owner )
   {
      ask_operation op;
      op.amount = quantity.amount;
      op.ask_index.order_price = price_per_unit;
      op.ask_index.owner = owner;

      operations.emplace_back(op);
   }

   void transaction::short_sell( const asset& quantity,
                          const price& price_per_unit,
                          const address& owner )
   {
      short_operation op;
      op.amount = quantity.amount;
      op.short_index.order_price = price_per_unit;
      op.short_index.owner = owner;

      operations.emplace_back(op);
   }

   void transaction::withdraw( const balance_id_type& account,
                               share_type             amount )
   { try {
      FC_ASSERT( amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.push_back( withdraw_operation( account, amount ) );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account",account)("amount",amount) ) }

   void transaction::withdraw_pay( const account_id_type& account,
                                   share_type             amount )
   {
      FC_ASSERT( amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.push_back( withdraw_pay_operation( amount, account ) );
   }

   void transaction::deposit( const address&  owner,
                              const asset&    amount,
                              slate_id_type   slate_id )
   {
      FC_ASSERT( amount.amount > 0, "amount: ${amount}", ("amount",amount) );
      operations.push_back( deposit_operation( owner, amount, slate_id ) );
   }

   void transaction::deposit_to_account( fc::ecc::public_key receiver_key,
                                         asset amount,
                                         fc::ecc::private_key from_key,
                                         const std::string& memo_message,
                                         slate_id_type slate_id,
                                         const fc::ecc::public_key& memo_pub_key,
                                         fc::ecc::private_key one_time_private_key,
                                         memo_flags_enum memo_type
                                         )
   {
      withdraw_with_signature by_account;
      by_account.encrypt_memo_data( one_time_private_key,
                                 receiver_key,
                                 from_key,
                                 memo_message,
                                 memo_pub_key,
                                 memo_type );

      deposit_operation op;
      op.amount = amount.amount;
      op.condition = withdraw_condition( by_account, amount.asset_id, slate_id );

      operations.push_back( op );
   }

   void transaction::register_account( const std::string& name,
                                       const fc::variant& public_data,
                                       const public_key_type& master,
                                       const public_key_type& active,
                                       uint8_t pay_rate )
   {
      const auto op = register_account_operation( name, public_data, master, active, pay_rate );
      operations.push_back( op );
   }

   void transaction::update_account( account_id_type account_id,
                                  uint8_t delegate_pay_rate,
                                  const fc::optional<fc::variant>& public_data,
                                  const fc::optional<public_key_type>& active   )
   {
      update_account_operation op;
      op.account_id = account_id;
      op.public_data = public_data;
      op.active_key = active;
      op.delegate_pay_rate = delegate_pay_rate;
      operations.push_back( op );
   }

#if 0
   void transaction::submit_proposal(account_id_type delegate_id,
                                     const std::string& subject,
                                     const std::string& body,
                                     const std::string& proposal_type,
                                     const fc::variant& public_data)
   {
     submit_proposal_operation op;
     op.submitting_delegate_id = delegate_id;
     op.submission_date = blockchain::now();
     op.subject = subject;
     op.body = body;
     op.proposal_type = proposal_type;
     op.data = public_data;
     operations.push_back(op);
   }

   void transaction::vote_proposal(proposal_id_type proposal_id,
                                   account_id_type voter_id,
                                   proposal_vote::vote_type vote,
                                   const string& message )
   {
     vote_proposal_operation op;
     op.id.proposal_id = proposal_id;
     op.id.delegate_id = voter_id;
     op.timestamp = blockchain::now();
     op.vote = vote;
     op.message = message;
     operations.push_back(op);
   }
#endif

   void transaction::create_asset( const std::string& symbol,
                                   const std::string& name,
                                   const std::string& description,
                                   const fc::variant& data,
                                   account_id_type issuer_id,
                                   share_type   max_share_supply,
                                   int64_t      precision )
   {
      FC_ASSERT( max_share_supply > 0 );
      FC_ASSERT( max_share_supply <= BTS_BLOCKCHAIN_MAX_SHARES );
      create_asset_operation op;
      op.symbol = symbol;
      op.name = name;
      op.description = description;
      op.public_data = data;
      op.issuer_account_id = issuer_id;
      op.maximum_share_supply = max_share_supply;
      op.precision = precision;
      operations.push_back( op );
   }

   void transaction::issue( const asset& amount_to_issue )
   {
      operations.push_back( issue_asset_operation( amount_to_issue ) );
   }
   void transaction::cover( const asset& cover_amount,
                            const market_index_key& order_idx )
   {
      operations.push_back( cover_operation(cover_amount.amount, order_idx) );
   }

   void transaction::add_collateral( share_type collateral_amount,
                                     const market_index_key& order_idx )
   {
      operations.push_back( add_collateral_operation(collateral_amount, order_idx) );
   }

   void transaction::publish_feed( feed_id_type feed_id,
                                   account_id_type delegate_id,
                                   fc::variant value )
   {
      operations.push_back( update_feed_operation{ feed_index{feed_id,delegate_id}, value } );
   }

   bool transaction::is_cancel()const
   {
      for( const auto& op : operations )
      {
          switch( operation_type_enum( op.type ) )
          {
              case bid_op_type:
                  if( op.as<bid_operation>().amount < 0 ) return true;
                  break;
              case ask_op_type:
                  if( op.as<ask_operation>().amount < 0 ) return true;
                  break;
              case short_op_type:
                  if( op.as<short_operation>().amount < 0 ) return true;
                  break;
              default:
                  break;
          }
      }
      return false;
   }

} } // bts::blockchain
