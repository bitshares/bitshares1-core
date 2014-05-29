#pragma once
#include <fc/network/ip.hpp>
#include <fc/variant.hpp>
#include <fc/time.hpp>

namespace bts { namespace api {

  fc::variant fc_ip_endpoint_to_variant(const fc::ip::endpoint& endpoint);
  fc::ip::endpoint variant_to_fc_ip_endpoint(const fc::variant& endpoint_as_variant);
  
  fc::variant time_interval_in_seconds_to_variant(const fc::microseconds& time_interval);
  fc::microseconds variant_to_time_interval_in_seconds(const fc::variant& time_interval_as_variant);

} } // end namespace bts::api