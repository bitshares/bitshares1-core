#pragma once
#include <fc/exception/exception.hpp>

namespace bts { namespace network {
   FC_DECLARE_EXCEPTION( network_exception, 80000, "Blockchain Exception" ); 
   FC_DECLARE_DERIVED_EXCEPTION( message_too_large, bts::network::network_exception, 80001, "message too large" ); 
} }
