#include <bts/api/conversion_functions.hpp>

namespace bts { namespace api {

fc::variant fc_ip_endpoint_to_variant(const fc::ip::endpoint& endpoint)
{
  return fc::variant(std::string(endpoint));
}

fc::ip::endpoint variant_to_fc_ip_endpoint(const fc::variant& endpoint_as_variant)
{
  return fc::ip::endpoint::from_string(endpoint_as_variant.as_string());
}


} } // end namespace bts::api