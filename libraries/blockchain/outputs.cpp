#include <bts/blockchain/outputs.hpp>
#include <fc/io/json.hpp>

namespace bts { namespace blockchain {

   const claim_type_enum claim_by_signature_output::type    = claim_type_enum::claim_by_signature;
   const claim_type_enum claim_by_pts_output::type          = claim_type_enum::claim_by_pts;
   const claim_type_enum claim_by_password_output::type     = claim_type_enum::claim_by_password;
   const claim_type_enum claim_by_multi_sig_output::type    = claim_type_enum::claim_by_multi_sig;
   const claim_type_enum claim_name_output::type            = claim_type_enum::claim_name;
   const claim_type_enum claim_fire_delegate_output::type  = claim_type_enum::claim_fire_delegate;


   const claim_type_enum claim_by_signature_input::type    = claim_type_enum::claim_by_signature;
   const claim_type_enum claim_by_multi_sig_input::type    = claim_type_enum::claim_by_multi_sig;

   /** valid names are selected to follow DNS naming conventions */
   bool claim_name_output::is_valid_name( const std::string& name )
   {
      if( name.size() == 0                ) return false;
      if( name[0] < 'a' || name [0] > 'z' ) return false;

      for( auto c : name )
      {
         if( c >= 'a' && c <= 'z' ) continue;
         else if( c >= '0' && c <= '9' ) continue;
         else if( c == '-' ) continue;
         else return false;
      }

      return true;
   }

   claim_name_output:: claim_name_output( std::string name_arg, 
                                          const fc::variant& data_arg, 
                                          uint32_t delegate_id_arg, 
                                          const fc::ecc::public_key_data& owner_arg,
                                          const fc::ecc::public_key_data& active_arg
                                          )
   :name( std::move(name_arg) ), 
    data( fc::json::to_string(data_arg) ), 
    delegate_id(delegate_id_arg), 
    owner( std::move(owner_arg) ),
    active( std::move(active_arg) )
   {}

} } // bts::blockchain
