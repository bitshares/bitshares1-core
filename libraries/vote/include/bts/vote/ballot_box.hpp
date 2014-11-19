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
   void clear();
   void close();

   void store_new_decision(const signed_voter_decision& decision);
   cast_decision get_decision(const digest_type& id);
   vector<digest_type> list_decisions(const digest_type& skip_to_id, uint32_t limit);
   vector<digest_type> get_decisions_by_voter(const address& voter);
   vector<digest_type> get_decisions_by_contest(const digest_type& contest_id);
   vector<digest_type> get_decisions_by_ballot(const digest_type& ballot_id);

   vector<string> get_all_write_ins();
   vector<digest_type> get_decisions_with_write_in(string write_in_name);

   void store_ballot(const vote::ballot& ballot);
   ballot get_ballot(const digest_type& id);
   vector<digest_type> list_ballots(const digest_type& skip_to_id, uint32_t limit);
   vector<digest_type> get_ballots_by_contest(const digest_type& contest_id);

   void store_contest(const vote::contest& contest);
   contest get_contest(const digest_type& id);
   vector<digest_type> list_contests(const digest_type& skip_to_id, uint32_t limit);
   vector<string> get_values_by_tag(string key);
   vector<digest_type> get_contests_by_tags(string key, string value);
   vector<digest_type> get_contests_by_contestant(string contestant);
};

} } // namespace bts::vote
