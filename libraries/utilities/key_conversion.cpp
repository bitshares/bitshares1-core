#include <bts/utilities/key_conversion.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/variant.hpp>

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
  digest = fc::sha256::hash(digest);
  memcpy(data + size_of_data_to_hash, (char*)&digest, size_of_hash_bytes);
  return fc::to_base58(data, sizeof(data));
}

/**
 * @deprecated - this is for backward compatibility of keys generated
 */
std::string key_to_wif_single_hash(const fc::ecc::private_key& key)
{
    fc::sha256 secret = key.get_secret();
    const size_t size_of_data_to_hash = sizeof(secret) + 1;
    const size_t size_of_hash_bytes = 4;
    char data[size_of_data_to_hash + size_of_hash_bytes];
    data[0] = (char)0x80;
    memcpy(&data[1], (char*)&secret, sizeof(secret));
    fc::sha256 digest = fc::sha256::hash(data, size_of_data_to_hash);
    digest = fc::sha256::hash(digest);
    memcpy(data + size_of_data_to_hash, (char*)&digest, size_of_hash_bytes);
    return fc::to_base58(data, sizeof(data));
}


fc::optional<fc::ecc::private_key> wif_to_key( const std::string& wif_key )
{
  std::vector<char> wif_bytes = fc::from_base58(wif_key);
  if (wif_bytes.size() < 5)
    return fc::optional<fc::ecc::private_key>();
  std::vector<char> key_bytes(wif_bytes.begin() + 1, wif_bytes.end() - 4);
  fc::ecc::private_key key = fc::variant(key_bytes).as<fc::ecc::private_key>();
  fc::sha256 check = fc::sha256::hash(wif_bytes.data(), wif_bytes.size() - 4);
  fc::sha256 check2 = fc::sha256::hash(check);
    
  if( memcmp( (char*)&check, wif_bytes.data() + wif_bytes.size() - 4, 4 ) == 0 || 
      memcmp( (char*)&check2, wif_bytes.data() + wif_bytes.size() - 4, 4 ) == 0 )
    return key;

  return fc::optional<fc::ecc::private_key>();
}

} } // end namespace bts::utilities
