#include <bts/blockchain/outputs.hpp>

namespace bts { namespace blockchain {

   const claim_type_enum claim_by_signature_output::type    = claim_type_enum::claim_by_signature;
   const claim_type_enum claim_by_pts_output::type          = claim_type_enum::claim_by_pts;
   const claim_type_enum claim_by_password_output::type     = claim_type_enum::claim_by_password;
   const claim_type_enum claim_by_multi_sig_output::type    = claim_type_enum::claim_by_multi_sig;
   const claim_type_enum claim_name_output::type            = claim_type_enum::claim_name;


   const claim_type_enum claim_by_signature_input::type    = claim_type_enum::claim_by_signature;
   const claim_type_enum claim_by_multi_sig_input::type    = claim_type_enum::claim_by_multi_sig;

} } // bts::blockchain
