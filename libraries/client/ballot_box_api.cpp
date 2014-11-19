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

cast_decision client_impl::ballot_get_decision(const fc::sha256& decision_id) const
{
   SANITY_CHECK;
   return _ballot_box->get_decision(decision_id);
}

vector<digest_type> client_impl::ballot_get_decisions_by_voter(const address& voter_address) const
{
   SANITY_CHECK;
   return _ballot_box->get_decisions_by_voter(voter_address);
}

vector<digest_type> client_impl::ballot_get_decisions_by_contest(const fc::sha256& contest_id) const
{
   SANITY_CHECK;
   return _ballot_box->get_decisions_by_contest(contest_id);
}

vector<digest_type> client_impl::ballot_get_decisions_by_ballot(const fc::sha256& ballot_id) const
{
   SANITY_CHECK;
   return _ballot_box->get_decisions_by_ballot(ballot_id);
}

vector<string> client_impl::ballot_get_all_write_in_candidates() const
{
   SANITY_CHECK;
   return _ballot_box->get_all_write_ins();
}

vector<digest_type> client_impl::ballot_get_decisions_with_write_in(const std::string& write_in_name) const
{
   SANITY_CHECK;
   return _ballot_box->get_decisions_with_write_in(write_in_name);
}

ballot client_impl::ballot_get_ballot_by_id(const fc::sha256& ballot_id) const
{
   SANITY_CHECK;
   return _ballot_box->get_ballot(ballot_id);
}

vector<digest_type> client_impl::ballot_get_ballots_by_contest(const fc::sha256& contest_id) const
{
   SANITY_CHECK;
   return _ballot_box->get_ballots_by_contest(contest_id);
}

contest client_impl::ballot_get_contest_by_id(const fc::sha256 &contest_id) const
{
   SANITY_CHECK;
   return _ballot_box->get_contest(contest_id);
}

vector<string> client_impl::ballot_get_tag_values_by_key(const string &key) const
{
   SANITY_CHECK;
   return _ballot_box->get_values_by_tag(key);
}

vector<digest_type> client_impl::ballot_get_contests_by_tag(const string &key, const string &value) const
{
   SANITY_CHECK;
   return _ballot_box->get_contests_by_tags(key, value);
}

vector<digest_type> client_impl::ballot_get_contests_by_contestant(const string &contestant_name) const
{
   SANITY_CHECK;
   return _ballot_box->get_contests_by_contestant(contestant_name);
}

} } } // namespace bts::client::detail
