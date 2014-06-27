#pragma once
#include <bts/blockchain/types.hpp>
#include <fc/network/ip.hpp>

namespace bts { namespace network {
   using namespace bts::blockchain;
   using fc::time_point;
   using bts::blockchain::public_key_type;
   using fc::ip::endpoint;

   struct peer_info
   {
       fc::ip::endpoint                   peer_endpoint;
       time_point                         connected_time;
       time_point                         peer_local_time;
       public_key_type                    peer_id;

       // delegates hosted on this peer
       vector<public_key_type>            delegate_ids;
   };

} }

FC_REFLECT( bts::network::peer_info,
            (peer_endpoint)
            (connected_time)
            (peer_local_time)
            (peer_id)
            (delegate_ids) 
          )
