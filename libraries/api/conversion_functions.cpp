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

fc::variant time_interval_in_seconds_to_variant(const fc::microseconds& time_interval)
{
  return time_interval.count() / fc::seconds(1).count();
}

fc::microseconds variant_to_time_interval_in_seconds(const fc::variant& time_interval_as_variant)
{
  return fc::seconds(time_interval_as_variant.as_int64());
}

} } // end namespace bts::api