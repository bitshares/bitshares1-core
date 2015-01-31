#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/utilities/words.hpp>
#include <bts/wallet/config.hpp>
#include <bts/wallet/exceptions.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>
#include <fc/network/url.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>

fc::array<char,25> create_random_bingo_board()
{
   auto secret = fc::ecc::private_key::generate().get_secret();
   vector<char> options(255);
   for( uint32_t i = 0; i < 255; ++i )
      options[i] = i;

   uint8_t* seedchar = (uint8_t*)&secret._hash[0];
   for( uint32_t i = 0; i < 255; ++i )
   {
       std::swap( options[i], options[seedchar[i%32]%255] );
       if( i == 31 )
       {
          secret = fc::sha256::hash(secret);
          seedchar = (uint8_t*)&secret._hash[0];
       }
   }
   fc::array<char,25> result;
   for( uint32_t i = 0; i < 25; ++i )
      result.at(i) = options[i];
   return result;
}

namespace bts { namespace client { namespace detail {

vector<uint8_t> detail::client_impl::bingo_fetch_balls( uint32_t first_block, uint32_t num_balls )
{
   vector<fc::ripemd160>  seeds =  _chain_db->fetch_random_seeds( first_block, first_block + num_balls -1 );
   vector<uint8_t> result; result.reserve(num_balls);
   for( auto item : seeds ) result.push_back( item._hash[0]%255 );

   FC_ASSERT( result.size() == num_balls );
   return result;
}

wallet_transaction_record detail::client_impl::wallet_bingo_buy_card( const string& paying_account_name,
                                                                      uint32_t max_balls, 
                                                                      const share_type& wager_amount,
                                                                      const string& wager_symbol )
{
   transaction_creation_state  creator( _chain_db );
   vector<public_key_type>  account_keys =  _wallet->get_public_keys_in_account( paying_account_name );

   for( auto key : account_keys ) creator.add_known_key( key );

   account_balance_record_summary_type balances = _wallet->get_spendable_account_balance_records( paying_account_name );

   for( auto item : balances )
   {
      for( auto balance : item.second )
          creator.pending_state._balance_id_to_record[balance.id()] = balance;
   }

   auto asset_rec = _chain_db->get_asset( wager_symbol );
   FC_ASSERT( asset_rec.valid() );

   asset fee = _wallet->get_transaction_fee( asset_rec->id );

   asset wager( wager_amount, asset_rec->id );
   if( wager.asset_id == fee.asset_id )
   {
      creator.withdraw( wager_amounti + fee );
   }
   else
   {
      creator.withdraw( wager_amount );
      creator.withdraw( fee );
   }
   withdraw_on_bingo bingo;
   bingo.owner = _wallet->get_active_public_key( paying_account_name );
   bingo.first_block = _chain_db->get_head_block_num() + 70;
   bingo.max_balls = max_balls;
   bingo.board = create_random_bingo_board();

   create.buy_bingo_card( bingo );
   create.pay_fee( fee );

   FC_ASSERT( !"Not Implemented" );
}

wallet_transaction_record detail::client_impl::wallet_bingo_claim( const bts::blockchain::address& balance_id )
{
   FC_ASSERT( !"Not Implemented" );
}
map<balance_id_type,balance_record>  detail::client_impl::wallet_bingo_cards( const string& account, bool filter_lost )
{
   return _wallet->get_bingo_cards( account, filter_lost );
}

    
}}}
