#pragma once

#include <string>
#include <fc/crypto/elliptic.hpp>
#include <fc/optional.hpp>

namespace bts { namespace utilities {

std::string                        key_to_wif_single_hash(const fc::ecc::private_key& key);
std::string                        key_to_wif(const fc::ecc::private_key& key);
fc::optional<fc::ecc::private_key> wif_to_key( const std::string& wif_key );

} } // end namespace bts::utilities
