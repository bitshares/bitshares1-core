#include <bts/btsx/outputs.hpp>

namespace bts { namespace btsx {

   const claim_type_enum claim_by_bid_output::type         = claim_type_enum::claim_by_bid;
   const claim_type_enum claim_by_long_output::type        = claim_type_enum::claim_by_long;
   const claim_type_enum claim_by_cover_output::type       = claim_type_enum::claim_by_cover;
   const claim_type_enum claim_by_opt_execute_output::type = claim_type_enum::claim_by_opt_execute;

} } // bts::btsx
