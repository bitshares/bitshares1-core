#pragma once

#include <fc/crypto/elliptic.hpp>
#include <fc/filesystem.hpp>

namespace bts { namespace bitcoin {

std::vector<fc::ecc::private_key> import_multibit_wallet( const fc::path& wallet_dat, const std::string& passphrase );

} } // bts::bitcoin
