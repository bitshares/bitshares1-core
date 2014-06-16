#pragma once
#include <fc/time.hpp>
#include <fc/optional.hpp>

namespace bts { namespace blockchain {


   fc::optional<fc::time_point> ntp_time();

   void                   advance_time( int32_t seconds );
   void                   start_simulated_time( const fc::time_point& sim_time );
   fc::time_point_sec     now();


} }
