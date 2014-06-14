#pragma once
#include <fc/reflect/variant.hpp>
#include <functional>
#include <string>
#include <fc/optional.hpp>
#include <fc/variant.hpp>

//#include <fc/network/ip.hpp>
//#include <fc/variant.hpp>

namespace bts { namespace api {

  enum method_prerequisites
  {
    no_prerequisites     = 0,
    json_authenticated   = 1,
    wallet_open          = 2,
    wallet_unlocked      = 4,
    connected_to_network = 8,
  };

  enum parameter_classification
  {
    required_positional,
    required_positional_hidden, /* Hide in help e.g. interactive password entry */
    optional_positional,
    optional_named
  };

  struct parameter_data
  {
    std::string name;
    std::string type;
    parameter_classification classification;
    fc::ovariant default_value;
    parameter_data(){}
    parameter_data(const parameter_data& rhs) :
      name(rhs.name),
      type(rhs.type),
      classification(rhs.classification),
      default_value(rhs.default_value)
    {}
    parameter_data(const parameter_data&& rhs) :
      name(std::move(rhs.name)),
      type(std::move(rhs.type)),
      classification(std::move(rhs.classification)),
      default_value(std::move(rhs.default_value))
    {}
    parameter_data(std::string name,
                    std::string type,
                    parameter_classification classification,
                    fc::ovariant default_value) :
      name(name),
      type(type),
      classification(classification),
      default_value(default_value)
    {}
    parameter_data& operator=(const parameter_data& rhs)
    {
      name = rhs.name;
      type = rhs.type;
      classification = rhs.classification;
      default_value = rhs.default_value;
      return *this;
    }
  };

  typedef std::function<fc::variant(const fc::variants& params)> json_api_method_type;

  struct method_data
  {
    std::string                 name;
    json_api_method_type        method;
    std::string                 description;
    std::string                 return_type;
    std::vector<parameter_data> parameters;
    uint32_t                    prerequisites;
    std::string                 detailed_description;
    std::vector<std::string>    aliases;
  };

} } // end namespace bts::api

FC_REFLECT_ENUM(bts::api::method_prerequisites, (no_prerequisites)(json_authenticated)(wallet_open)(wallet_unlocked)(connected_to_network))
FC_REFLECT_ENUM( bts::api::parameter_classification, (required_positional)(required_positional_hidden)(optional_positional)(optional_named) )
FC_REFLECT( bts::api::parameter_data, (name)(type)(classification)(default_value) )
FC_REFLECT( bts::api::method_data, (name)(description)(return_type)(parameters)(prerequisites)(detailed_description)(aliases) )
