#pragma once
#include <fc/time.hpp>

namespace bts { namespace blockchain {


   void               advance_time( int32_t seconds );
   fc::time_point_sec now();


} }
