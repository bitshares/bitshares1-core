#pragma once

#include <bts/vote/types.hpp>

#include <fc/filesystem.hpp>

#include <memory>

namespace bts { namespace vote {
namespace detail { class ballot_box_impl; }

class ballot_box
{
   std::shared_ptr<detail::ballot_box_impl> my;

public:
   ballot_box();

   void open(fc::path data_dir);
   bool is_open();
   void close();

   void store_new_decision(const signed_voter_decision& decision);
   signed_voter_decision get_decision(const digest_type& id);
   vector<digest_type> get_decisions_by_voter(const address& voter);
   vector<digest_type> get_decisions_by_contest(const digest_type& contest_id);
   vector<digest_type> get_decisions_by_ballot(const digest_type& ballot_id);

   vector<string> get_all_write_ins();
   vector<digest_type> get_decisions_with_write_in(string write_in_name);

   ballot get_ballot(const digest_type& id);
   vector<digest_type> get_ballots_by_contest(const digest_type& contest_id);
};

} } // namespace bts::vote
