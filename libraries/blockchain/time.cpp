#include <bts/blockchain/time.hpp>
#include <fc/network/ntp.hpp>
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

static int32_t simulated_time    = 0;
static int32_t adjusted_time_sec = 0;

fc::optional<fc::time_point> ntp_time()
{
   static fc::ntp* ntp_service = new fc::ntp();
   return ntp_service->get_time();
}

fc::time_point_sec now()
{
   if( simulated_time ) return fc::time_point() + fc::seconds(simulated_time + adjusted_time_sec);

   auto ntp = ntp_time();
   if( ntp )
      return *ntp + fc::seconds( adjusted_time_sec );
   else
      return fc::time_point::now() + fc::seconds( adjusted_time_sec );
}

double ntp_error()
{
  FC_ASSERT(ntp_time(), "We don't have NTP time, we can't calculate error");
  return ( *ntp_time() - fc::time_point::now() ).count() / double( 1000000 );
}

void start_simulated_time( const fc::time_point& sim_time )
{
   simulated_time = sim_time.sec_since_epoch();
   adjusted_time_sec = 0;
}

void advance_time( int32_t delta_seconds )
{
  adjusted_time_sec += delta_seconds;
}

} } // bts::blockchain
