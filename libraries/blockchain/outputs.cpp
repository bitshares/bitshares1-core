#include <bts/blockchain/outputs.hpp>
#include <fc/io/json.hpp>

namespace bts { namespace blockchain {

   const claim_type_enum claim_by_signature_output::type    = claim_type_enum::claim_by_signature;
   const claim_type_enum claim_by_pts_output::type          = claim_type_enum::claim_by_pts;
   const claim_type_enum claim_by_password_output::type     = claim_type_enum::claim_by_password;
   const claim_type_enum claim_by_multi_sig_output::type    = claim_type_enum::claim_by_multi_sig;
   const claim_type_enum claim_name_output::type            = claim_type_enum::claim_name;


   const claim_type_enum claim_by_signature_input::type    = claim_type_enum::claim_by_signature;
   const claim_type_enum claim_by_multi_sig_input::type    = claim_type_enum::claim_by_multi_sig;

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
   claim_name_output:: claim_name_output( std::string n, const fc::variant& d, 
                                          uint32_t did, fc::ecc::public_key own )
   :name( std::move(n) ), data( fc::json::to_string(d) ), delegate_id(did), owner( std::move(own) ){}

} } // bts::blockchain
