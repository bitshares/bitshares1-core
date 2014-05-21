#include <bts/utilities/key_conversion.hpp>
#include <fc/crypto/base58.hpp>

namespace bts { namespace utilities {

std::string key_to_wif(const fc::ecc::private_key& key)
{
  fc::sha256 secret = key.get_secret();
  const size_t size_of_data_to_hash = sizeof(secret) + 1;
  const size_t size_of_hash_bytes = 4;
  char data[size_of_data_to_hash + size_of_hash_bytes];
  data[0] = (char)0x80;
  memcpy(&data[1], (char*)&secret, sizeof(secret));
  fc::sha256 digest = fc::sha256::hash(data, size_of_data_to_hash);
  memcpy(data + size_of_data_to_hash, (char*)&digest, size_of_hash_bytes);
  return fc::to_base58(data, sizeof(data));
}

} } // end namespace bts::utilities