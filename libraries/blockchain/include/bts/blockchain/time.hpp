#pragma once

#include <fc/optional.hpp>
#include <fc/signals.hpp>
#include <fc/time.hpp>
#include <string>

namespace bts { namespace blockchain {

   typedef fc::signal<void()> time_discontinuity_signal_type;
   extern time_discontinuity_signal_type time_discontinuity_signal;

   fc::optional<fc::time_point> ntp_time();
   fc::time_point_sec           now();
   fc::time_point_sec           nonblocking_now(); // identical to now() but guaranteed not to block
   void                         update_ntp_time();
   fc::microseconds             ntp_error();
   void                         shutdown_ntp_time();

   void                         start_simulated_time( const fc::time_point sim_time );
   void                         advance_time( int32_t delta_seconds );

   uint32_t                     get_slot_number( const fc::time_point_sec timestamp );
   fc::time_point_sec           get_slot_start_time( uint32_t slot_number );
   fc::time_point_sec           get_slot_start_time( const fc::time_point_sec timestamp );

   // drop-in replacements for fc library functions
   // which respect simulated_time
   std::string get_approximate_relative_time_string( const fc::time_point_sec& event_time );
   std::string get_approximate_relative_time_string( const fc::time_point& event_time );

} } // bts::blockchain
