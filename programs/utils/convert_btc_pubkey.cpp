#include <bts/blockchain/address.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/openssl.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/io/raw.hpp>
#include <iostream>
#include <fstream>


#include <assert.h>

using namespace bts::blockchain;

int main( int argc, char** argv )
{
 if( argc == 1 )
 {
  std::cout << "usage: convert_btc_pubkey <btc public key | not address>" <<"\n";
 }
 else
 {
  fc::array<char, 65> data;
  fc::ecc::public_key_data mypubkey;
  std::string myarg = argv[1];
  fc::from_hex(myarg, (char*)&data, sizeof(data) );
  memcpy( (char*)mypubkey.data, data.data, sizeof(mypubkey) );
  std::cout << "Please verify the Bitcoin address:\n";
  std::cout << "BTC Address (compressed):  " << std::string(pts_address( mypubkey, true, 0 )) <<"\n";
  std::cout << "BTSX public key:        :  " << std::string(public_key_type(mypubkey)) <<"\n";
 }
}
