#include <bts/wallet/wallet_db.hpp>
#include <fc/crypto/aes.hpp>

#include <fc/log/logger.hpp>

namespace bts{ namespace wallet {

  const uint32_t master_key_record::type           =   master_key_record_type; 
  const uint32_t private_key_record::type          =   private_key_record_type; 
  const uint32_t wallet_name_record::type          =   name_record_type; 
  const uint32_t wallet_asset_record::type         =   asset_record_type; 
  const uint32_t wallet_balance_record::type       =   balance_record_type; 
  const uint32_t wallet_transaction_record::type   =   transaction_record_type; 
  const uint32_t wallet_account_record::type       =   account_record_type; 
  const uint32_t wallet_meta_record::type          =   meta_record_type; 

  address_index wallet_account_record::get_next_key_index( uint32_t invoice_number )
  {
      auto invoice_itr = last_payment_index.find( invoice_number );
      if( invoice_itr == last_payment_index.end() )
      {
         last_payment_index[invoice_number] = 0;
         return address_index( account_number, invoice_number, 0 );
      }
      else
      {
         return address_index( account_number, invoice_number, ++invoice_itr->second );
      }
  }
  int32_t wallet_account_record::get_next_invoice_index()
  {
      auto last_itr = last_payment_index.rbegin();
      if( last_itr == last_payment_index.rend() || last_itr->first < 0 )
      {
         last_payment_index[1] = 0;
         return 1;
      }
      last_payment_index[last_itr->first + 1] = 0;
      return last_itr->first + 1;
  }
  public_key_type wallet_account_record::get_key( int32_t invoice_number, int32_t payment_number )const
  {
     return fc::ecc::public_key(extended_key.child( invoice_number ).child( payment_number ));
  }


  extended_private_key    master_key_record::get_extended_private_key( const fc::sha512& password )const
  { try {
     return fc::raw::unpack<extended_private_key>( fc::aes_decrypt( password, encrypted_key ) );
  } FC_RETHROW_EXCEPTIONS( warn, "" ) }


  fc::ecc::private_key    private_key_record::get_private_key( const fc::sha512& password )const
  { try {
     return fc::raw::unpack<fc::ecc::private_key>( fc::aes_decrypt( password, encrypted_key ) );
  } FC_RETHROW_EXCEPTIONS( warn, "" ) }
  

  private_key_record::private_key_record( int32_t index_arg, int32_t account_number_arg, int32_t extra_index_arg,
                                          const fc::ecc::private_key& key, const fc::sha512& password )
  {
     index = index_arg;
     account_number = account_number_arg;
     extra_key_index = extra_index_arg;
     encrypted_key = fc::aes_encrypt( password, fc::raw::pack( key ) );
  }

} } // bts::wallet
