#pragma once

#include <string>
#include <fc/crypto/elliptic.hpp>

namespace bts { namespace utilities {

std::string key_to_wif(const fc::ecc::private_key& key);

} } // end namespace bts::utilities