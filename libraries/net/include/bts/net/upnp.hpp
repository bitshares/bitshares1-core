#pragma once
#include <stdint.h>
#include <memory>
#include <fc/network/ip.hpp>

namespace bts { namespace net {

  namespace detail {  class upnp_service_impl; }

  class upnp_service 
  {
    public:
      upnp_service();
      ~upnp_service();

      fc::ip::address external_ip()const;
      uint16_t mapped_port()const;
      void map_port( uint16_t p );
    private:
      std::unique_ptr<detail::upnp_service_impl> my;
  };

} } // bts::net
