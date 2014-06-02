#include <algorithm>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/filesystem.hpp>

namespace bts { namespace bitcoin {

std::vector<fc::ecc::private_key> import_bitcoin_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{ try {
  FC_ASSERT( !"Support for Importing Bitcoin Core wallets was not compiled in.", "Unable to load wallet ${wallet}", ("wallet",wallet_dat) );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

std::vector<fc::ecc::private_key> import_armory_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{ try {
  FC_ASSERT( !"Support for Importing Bitcoin Core wallets was not compiled in.", "Unable to load wallet ${wallet}", ("wallet",wallet_dat) );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

std::vector<fc::ecc::private_key> import_electrum_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{ try {
  FC_ASSERT( !"Support for Importing Bitcoin Core wallets was not compiled in.", "Unable to load wallet ${wallet}", ("wallet",wallet_dat) );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

std::vector<fc::ecc::private_key> import_multibit_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{ try {
  FC_ASSERT( !"Support for Importing Bitcoin Core wallets was not compiled in.", "Unable to load wallet ${wallet}", ("wallet",wallet_dat) );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

} } // bts::bitcoin
