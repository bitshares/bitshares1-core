#include <bts/blockchain/transaction_creation_state.hpp>
#include <bts/blockchain/balance_operations.hpp>

#include <algorithm>

namespace bts { namespace blockchain {
 
    transaction_creation_state::transaction_creation_state( chain_interface_ptr prev_state )
    :pending_state(prev_state),required_signatures(1),eval_state(&pending_state)
    {
       
    }

    void transaction_creation_state::withdraw( const asset& amount_to_withdraw )
    {
       share_type left_to_withdraw = amount_to_withdraw.amount;
       for( auto item : pending_state._balance_id_to_record )
       {
          if( item.second.asset_id() == amount_to_withdraw.asset_id )
          {
             auto owner = item.second.owner();
             if( owner && eval_state.signed_keys.find(*owner) != eval_state.signed_keys.end() )
             {
                auto withdraw_from_balance = std::min<share_type>( item.second.balance, left_to_withdraw );
                left_to_withdraw    -= withdraw_from_balance;

                trx.withdraw( item.first, withdraw_from_balance );

                eval_state.evaluate_operation( trx.operations.back() );

                required_signatures.front().owners.insert( *owner );
                required_signatures.front().required = required_signatures.front().owners.size();

                if( left_to_withdraw == 0 )
                   break;
             }
          }
       }
       FC_ASSERT( left_to_withdraw == 0 );
    }

    void transaction_creation_state::add_known_key( const address& key )
    {
      eval_state.signed_keys.insert(key); 
    }

    public_key_type transaction_creation_state::deposit( const asset&           amount, 
                                              const public_key_type&            to, 
                                              slate_id_type                     slate,
                                              const optional<private_key_type>& one_time_key,
                                              const string&                     memo,
                                              const optional<private_key_type>& from,
                                              bool                              stealth )
    {
       public_key_type receive_key = to;
       if( !one_time_key )
       {
          trx.deposit( to, amount, slate );
       }
       else
       {
          withdraw_with_signature by_account;
          receive_key = by_account.encrypt_memo_data( *one_time_key, 
                                                      to,
                                                      from ? *from 
                                                           : fc::ecc::private_key(), 
                                                      memo, 
                                                      one_time_key->get_public_key(), 
                                                      to_memo, 
                                                      stealth );

          deposit_operation op;
          op.amount = amount.amount;
          op.condition = withdraw_condition( by_account, amount.asset_id, slate );
          trx.operations.emplace_back(op);

       }
       eval_state.evaluate_operation( trx.operations.back() );
       return receive_key;
    }

    void transaction_creation_state::pay_fee( const asset& amount )
    {
        // TODO
    }

} }
