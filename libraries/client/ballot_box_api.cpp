#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/mail/message.hpp>
#include <bts/vote/messages.hpp>

namespace bts { namespace client { namespace detail {
using namespace bts::vote;

#define SANITY_CHECK FC_ASSERT( _ballot_box, "Ballot Box must be enabled before using its API!" )

void client_impl::ballot_submit_decision(const signed_voter_decision& decision)
{
   SANITY_CHECK;
   _ballot_box->store_new_decision(decision);
}

signed_voter_decision client_impl::ballot_get_decision(const fc::sha256& decision_id) const
{
   SANITY_CHECK;
   return _ballot_box->get_decision(decision_id);
}

vector<string> client_impl::ballot_get_all_write_in_candidates() const
{
   SANITY_CHECK;
   return _ballot_box->get_all_write_ins();
}

vector<digest_type> client_impl::ballot_get_decisions_with_write_in(const std::string &write_in_name) const
{
   SANITY_CHECK;
   return _ballot_box->get_decisions_with_write_in(write_in_name);
}

} } } // namespace bts::client::detail
