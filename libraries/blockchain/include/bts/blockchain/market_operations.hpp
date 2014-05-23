#pragma once
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

   struct bid_operation
   {
        static const operation_type_enum type; 
        bid_operation():amount(0){}

        share_type   amount;
        price        bid_price;
        address      owner;
        name_id_type delegate_id;
   };

   struct ask_operation
   {
        static const operation_type_enum type; 
        ask_operation():amount(0){}

        share_type   amount;
        price        ask_price;
        address      owner;
        name_id_type delegate_id;
   };

   struct short_operation
   {
        static const operation_type_enum type; 
        short_operation():amount(0){}

        share_type   amount;
        price        short_price;
        address      owner;
        name_id_type delegate_id;
   };
   
   struct cover_operation
   {
        static const operation_type_enum type; 
        cover_operation():amount(0){}

        share_type   amount;
        price        cover_ask_price;
        address      owner;
        name_id_type delegate_id;
   };

   struct add_collateral_operation
   {
        static const operation_type_enum type; 
        add_collateral_operation():amount(0){}

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };

   struct remove_collateral_operation
   {
        static const operation_type_enum type; 
        remove_collateral_operation():amount(0){}

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::bid_operation, (amount)(bid_price)(owner)(delegate_id) )
FC_REFLECT( bts::blockchain::ask_operation, (amount)(ask_price)(owner)(delegate_id) )
FC_REFLECT( bts::blockchain::short_operation, (amount)(short_price)(owner)(delegate_id) )
FC_REFLECT( bts::blockchain::cover_operation, (amount)(cover_ask_price)(owner)(delegate_id) )
FC_REFLECT( bts::blockchain::add_collateral_operation, (amount)(owner)(delegate_id) )
FC_REFLECT( bts::blockchain::remove_collateral_operation, (amount)(owner)(delegate_id) )
