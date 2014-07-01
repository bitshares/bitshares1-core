#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/network/ntp.hpp>
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

static int32_t simulated_time    = 0;
static int32_t adjusted_time_sec = 0;

time_discontinuity_signal_type time_discontinuity_signal;

fc::optional<fc::time_point> ntp_time()
{
   static fc::ntp* ntp_service = nullptr;
   if( ntp_service == nullptr )
   {
      // TODO: allocate, then atomic swap and
      // free on failure... remove sync calls from
      // constructor of ntp() and into a "start" method
      // that is called only if we get a successful 
      // atomic swap.
      ntp_service = new fc::ntp();
   }
   return ntp_service->get_time();
}

fc::time_point_sec now()
{
   if( simulated_time )
       return fc::time_point() + fc::seconds( simulated_time + adjusted_time_sec );

   auto ntp = ntp_time();
   if( ntp.valid() )
      return *ntp + fc::seconds( adjusted_time_sec );
   else
      return fc::time_point::now() + fc::seconds( adjusted_time_sec );
}

fc::microseconds ntp_error()
{
   FC_ASSERT( ntp_time().valid(), "We don't have NTP time!" );
   return *ntp_time() - fc::time_point::now();
}

void start_simulated_time( const fc::time_point& sim_time )
{
   simulated_time = sim_time.sec_since_epoch();
   adjusted_time_sec = 0;
}

void advance_time( int32_t delta_seconds )
{
   adjusted_time_sec += delta_seconds;
   time_discontinuity_signal();
}

uint32_t get_slot_number( const fc::time_point_sec& timestamp )
{
   return timestamp.sec_since_epoch() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
}

fc::time_point_sec get_slot_start_time( uint32_t slot_number )
{
   return fc::time_point_sec( slot_number * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC );
}

fc::time_point_sec get_slot_start_time( const fc::time_point_sec& timestamp )
{
   return get_slot_start_time( get_slot_number( timestamp ) );
}

} } // bts::blockchain
