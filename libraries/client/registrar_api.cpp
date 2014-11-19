#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/mail/message.hpp>
#include <bts/vote/messages.hpp>

#include <fc/crypto/digest.hpp>

namespace bts { namespace client { namespace detail {
using namespace bts::vote;

expiring_signature client_impl::registrar_demo_registration(const signed_identity_property& validated_ballot_id,
                                                        const address& identity_address,
                                                        const fc::ecc::compact_signature& ballot_id_signature)
{
#ifdef BTS_TEST_NETWORK
   auto property_id = validated_ballot_id.id(identity_address);
   auto voter_key = fc::ecc::public_key(ballot_id_signature, property_id);

   std::unordered_set<address> signatories;
   const std::unordered_set<address> recognized_verifiers = {
      bts::blockchain::public_key_type("XTS6LNgKuUmEH18TxXWDEeqMtYYQBBXWfE1ZDdSx95jjCJvnwnoGy")
   };
   std::vector<address> recognized_signatories;

   fc::time_point_sec from_time;
   fc::time_point_sec expiration_time;
   for( const expiring_signature& signature : validated_ballot_id.verifier_signatures )
   {
      if( from_time == fc::time_point_sec() || from_time < signature.valid_from )
         from_time = signature.valid_from;
      if( expiration_time == fc::time_point_sec() || expiration_time > signature.valid_until )
         expiration_time = signature.valid_until;
      signatories.insert(signature.signer(property_id));
   }
   std::set_intersection(signatories.begin(), signatories.end(),
                         recognized_verifiers.begin(), recognized_verifiers.end(),
                         std::back_inserter(recognized_signatories));
   FC_ASSERT(recognized_signatories.size() > recognized_verifiers.size() / 2,
             "Ballot ID must be verfied by strictly more than half of the recognized verifiers!");

   return ballot::authorize_voter(validated_ballot_id.value.as<digest_type>(),
                                  _wallet->get_active_private_key("registrar"),
                                  voter_key, from_time, expiration_time);
#else
   FC_THROW("OI! This is a demo function, you clodpole! You can't call this in production!");
#endif
}

} } } // namespace bts::client::detail
