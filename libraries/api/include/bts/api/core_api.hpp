#pragma once
#include <bts/api/wallet_api.hpp>
#include <bts/api/network_api.hpp>

namespace bts { namespace api {

  class common_api : public wallet_api,
                     public network_api
  {
  };

} } // end namespace bts::api