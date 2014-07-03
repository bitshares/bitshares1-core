#pragma once
#include <fc/exception/exception.hpp>

namespace bts { namespace net {
   // registered in node.cpp 
   
   FC_DECLARE_EXCEPTION( net_exception, 90000, "P2P Networking Exception" ); 
   FC_DECLARE_DERIVED_EXCEPTION( send_queue_overflow, bts::net::net_exception, 90001, "send queue for this peer exceeded maximum size" ); 
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_priority_fee,  bts::net::net_exception, 90002, "insufficient priority fee" );

} }
