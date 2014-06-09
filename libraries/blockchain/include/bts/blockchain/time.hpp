#pragma once
#include <fc/time.hpp>

namespace bts { namespace blockchain {


   void               advance_time( int32_t seconds );
   void               start_simulated_time( const fc::time_point& sim_time );
   fc::time_point_sec now();


} }
