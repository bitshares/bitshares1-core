#include <bts/vote/ballot_box.hpp>
#include <bts/db/level_map.hpp>

#include <fc/reflect/variant.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

namespace bts { namespace vote {
using namespace bts::db;
using namespace fc;
using namespace boost::multi_index;

namespace detail {
class ballot_box_impl {
protected:
   //Dummy types to tag multi_index_container indices
   struct by_id;
   struct by_voter;
   struct by_contest;
   struct by_ballot;
   struct by_tags;
   struct by_latest;
public:
   struct decision_storage_record : public signed_voter_decision
   {
      decision_storage_record(){}
      decision_storage_record(const signed_voter_decision& s)
         : signed_voter_decision(s)
      {}

      operator cast_decision()
      {
         return { digest(),
                  contest_id, ballot_id, write_in_names,
                  voter_opinions, voter_key, timestamp,
                  authoritative, latest };
      }

      bts::blockchain::public_key_type voter_key = voter_public_key();
      fc::time_point timestamp = fc::time_point::now();
      bool authoritative;
      bool latest;
   };
   struct decision_index_record
   {
      decision_index_record(const decision_storage_record& s = decision_storage_record())
         : decision_index_record(s.digest(), s){}
      decision_index_record(const digest_type& id, const decision_storage_record& s = decision_storage_record())
         : id(id),
           voter(s.voter_key),
           contest_id(s.contest_id),
           latest(s.latest),
           write_in_names(s.write_in_names),
           ballot_id(s.ballot_id)
      {}

      digest_type id;
      address voter;
      digest_type contest_id;
      bool latest;
      vector<string> write_in_names;
      digest_type ballot_id;
   };
   level_map<digest_type, decision_storage_record> decision_db;
   multi_index_container<
      decision_index_record,
      indexed_by<
         //Constant-time lookup by ID
         hashed_unique<
            tag<by_id>,
            member<decision_index_record, digest_type, &decision_index_record::id>,
            std::hash<digest_type>
         >,
         //Constant-time lookup by voter
         hashed_non_unique<
            tag<by_voter>,
            member<decision_index_record, address, &decision_index_record::voter>,
            std::hash<address>
         >,
         //Constant-time lookup by contest
         hashed_non_unique<
            tag<by_contest>,
            member<decision_index_record, digest_type, &decision_index_record::contest_id>,
            std::hash<digest_type>
         >,
         //Constant-time lookup by ballot
         hashed_non_unique<
            tag<by_ballot>,
            member<decision_index_record, digest_type, &decision_index_record::ballot_id>,
            std::hash<digest_type>
         >,
         //Constant-time lookup for latest vote
         hashed_non_unique<
            tag<by_latest>,
            composite_key<
               decision_index_record,
               member<decision_index_record, address, &decision_index_record::voter>,
               member<decision_index_record, digest_type, &decision_index_record::contest_id>,
               member<decision_index_record, bool, &decision_index_record::latest>
            >,
            composite_key_hash<
               std::hash<address>,
               std::hash<digest_type>,
               boost::hash<bool>
            >
         >
      >
   > decision_index;
   std::multimap<string, digest_type> write_in_index;

   typedef ballot ballot_storage_record;
   level_map<digest_type, ballot_storage_record> ballot_db;
   std::multimap<digest_type, digest_type> ballot_by_contest_index;

   typedef contest contest_storage_record;
   struct contest_tags_index_record {
      string key;
      string value;
      digest_type contest_id;
   };
   level_map<digest_type, contest_storage_record> contest_db;
   multi_index_container<
      contest_tags_index_record,
      indexed_by<
         ordered_non_unique<
            tag<by_tags>,
            composite_key<
               contest_tags_index_record,
               member<contest_tags_index_record, string, &contest_tags_index_record::key>,
               member<contest_tags_index_record, string, &contest_tags_index_record::value>
            >
         >
      >
   > contest_tags_index;
   std::multimap<string, digest_type> contest_by_contestant_index;

   bool databases_open = false;

   ~ballot_box_impl()
   {
      if( databases_open )
         close();
   }

   void check_authority(decision_storage_record& decision)
   {
      decision.authoritative = false;

      try {
         //FIXME: Need a real way to know this
         const std::unordered_set<address> recognized_registrars = {
            public_key_type("XTS6pBHGAjnGrYmYKX9Ko26nQokqZf41YcX8FcuCb7zQrLQSUMnS8")
         };
         std::unordered_set<address> signatories;
         std::vector<address> recognized_signatories;

         //The decision references a ballot ID and a contest ID. We need to lookup that ballot and verify that the
         //contest is on it. If so, we need to check the registrar signatures authorize the voter to vote on the ballot
         auto ballot = ballot_db.fetch_optional(decision.ballot_id);
         if( ballot && std::find(ballot->contests.begin(),
                                 ballot->contests.end(),
                                 decision.contest_id) != ballot->contests.end() )
            for( const auto& signature : decision.registrar_authorizations )
               signatories.insert(ballot->get_authorizing_registrar(decision.ballot_id, signature, decision.voter_key));

         std::set_intersection(recognized_registrars.begin(), recognized_registrars.end(),
                               signatories.begin(), signatories.end(),
                               std::back_inserter(recognized_signatories));

         decision.authoritative = recognized_signatories.size() > recognized_registrars.size() / 2;
      } catch (...){
         /* In case of error in here, we just declare the decision non-authoritative. */
         elog("Error while checking authority of decision ${d}", ("d", decision));
      }
   }
   void supercede_latest(decision_storage_record& decision)
   {
      decision.latest = true;

      try {
         auto& index = decision_index.get<by_latest>();
         //Should only ever be one record found, but just in case...
         auto range = index.equal_range(boost::make_tuple(address(decision.voter_key), decision.contest_id, true));
         vector<std::pair<digest_type,decision_storage_record>> updated_storage_records;

         for( auto itr = range.first; itr != range.second; ++itr ) {
            auto storage_record = decision_db.fetch(itr->id);
            storage_record.latest = false;
            decision_db.store(itr->id, storage_record);
            updated_storage_records.push_back(std::make_pair(itr->id,storage_record));
         }
         while( !updated_storage_records.empty() )
         {
            update_index(updated_storage_records.back().first, updated_storage_records.back().second);
            updated_storage_records.pop_back();
         }
      } catch (...){
         elog("Error while updating latest decision ${d}", ("d", decision));
      }
   }

   void update_index(const digest_type& id, const decision_storage_record& storage_record)
   {
      decision_index.insert(decision_index_record(id, storage_record));
      for( auto write_in : storage_record.write_in_names )
         write_in_index.insert(std::make_pair(write_in, id));
   }
   void update_index(const digest_type& id, const ballot_storage_record& ballot_record)
   {
      for( auto contest : ballot_record.contests )
         ballot_by_contest_index.insert(std::make_pair(contest, id));
   }
   void update_index(const digest_type &id, const contest_storage_record& contest_record)
   {
      for( auto contestant : contest_record.contestants )
         contest_by_contestant_index.insert(std::make_pair(contestant.name, id));
      for( auto tag_pair : contest_record.tags )
         contest_tags_index.insert({tag_pair.first, tag_pair.second, id});
   }

   void store_record(const signed_voter_decision& decision)
   {
      auto id = decision.digest();
      decision_storage_record record(decision);
      check_authority(record);
      supercede_latest(record);
      decision_db.store(id, record);
      update_index(id, record);
   }
   void store_record(const vote::ballot& ballot)
   {
      auto id = ballot.id();
      ballot_db.store(id, ballot);
      update_index(id, ballot);
   }
   void store_record(const vote::contest& contest)
   {
      auto id = contest.id();
      contest_db.store(id, contest);
      update_index(id, contest);
   }

   void open(fc::path data_dir)
   {
      FC_ASSERT( !databases_open, "Refusing to open already-opened ballot box." );

      decision_db.open(data_dir / "decision_db");
      for( auto itr = decision_db.begin(); itr.valid(); ++itr )
         update_index(itr.key(), itr.value());

      ballot_db.open(data_dir / "ballot_db");
      for( auto itr = ballot_db.begin(); itr.valid(); ++itr )
         update_index(itr.key(), itr.value());

      contest_db.open(data_dir / "contest_db");
      for( auto itr = contest_db.begin(); itr.valid(); ++itr )
         update_index(itr.key(), itr.value());

      databases_open = true;
   }
   void clear()
   {
      if( databases_open )
      {
         for( auto itr = decision_db.begin(); itr.valid(); ++itr )
            decision_db.remove(itr.key());
         for( auto itr = ballot_db.begin(); itr.valid(); ++itr )
            ballot_db.remove(itr.key());
         for( auto itr = contest_db.begin(); itr.valid(); ++itr )
            contest_db.remove(itr.key());
      }

      decision_index.clear();
      write_in_index.clear();
      ballot_by_contest_index.clear();
      contest_by_contestant_index.clear();
      contest_tags_index.clear();
   }
   void close()
   {
      FC_ASSERT( databases_open, "Cannot close unopened ballot box." );

      databases_open = false;

      decision_db.close();
      ballot_db.close();
      contest_db.close();

      clear();
   }

   template<typename Index, typename Key>
   vector<digest_type> get_decisions_by_key(const Key& key)
   {
      vector<digest_type> results;
      auto range = decision_index.get<Index>().equal_range(key);
      std::for_each(range.first, range.second, [&results](const decision_index_record& rec) {
         results.push_back(rec.id);
      });
      return results;
   }
   vector<digest_type> list_decisions(const digest_type& skip_to_id, uint32_t limit)
   {
      vector<digest_type> results;
      auto& index = decision_index.get<by_id>();
      auto itr = index.begin();
      if( skip_to_id != digest_type() )
      {
         itr = index.find(skip_to_id);
         if( itr != index.end() )
            ++itr;
      }

      while( results.size() < limit && itr != index.end() )
         results.emplace_back(itr++->id);

      return results;
   }
   vector<digest_type> get_decisions_by_voter(const address& voter)
   {
      return get_decisions_by_key<by_voter>(voter);
   }
   vector<digest_type> get_decisions_by_contest(const digest_type& contest_id)
   {
      return get_decisions_by_key<by_contest>(contest_id);
   }
   vector<digest_type> get_decisions_by_ballot(const digest_type& ballot_id)
   {
      return get_decisions_by_key<by_ballot>(ballot_id);
   }

   vector<string> get_all_write_ins()
   {
      vector<string> results;
      for( auto itr = write_in_index.begin();
           itr != write_in_index.end();
           itr = write_in_index.upper_bound(itr->first) )
         results.push_back(itr->first);
      return results;
   }
   vector<digest_type> get_decisions_with_write_in(string write_in_name)
   {
      vector<digest_type> results;
      auto range = write_in_index.equal_range(write_in_name);
      std::for_each(range.first, range.second, [&results](const std::pair<string,digest_type>& rec) {
         results.push_back(rec.second);
      });
      return results;
   }

   template<typename LevelDB>
   vector<digest_type> list_leveldb(LevelDB& db, const digest_type& skip_to_id, uint32_t limit)
   {
      vector<digest_type> results;
      auto itr = db.begin();
      if( skip_to_id != digest_type() )
      {
         itr = db.find(skip_to_id);
         if( itr.valid() )
            ++itr;
      }

      while( results.size() < limit && itr.valid() )
      {
         results.emplace_back(itr.key());
         ++itr;
      }

      return results;
   }

   vector<digest_type> get_ballots_by_contest(const digest_type& contest_id)
   {
      vector<digest_type> results;
      auto range = ballot_by_contest_index.equal_range(contest_id);
      std::for_each(range.first, range.second, [&results](const std::pair<digest_type,digest_type>& rec) {
         results.push_back(rec.second);
      });
      return results;
   }

   vector<string> get_values_by_tag(string key)
   {
      vector<string> results;
      for( auto itr = contest_tags_index.lower_bound(boost::make_tuple(key));
           itr != contest_tags_index.upper_bound(boost::make_tuple(key));
           itr = contest_tags_index.upper_bound(boost::make_tuple(itr->key, itr->value)))
         results.push_back(itr->value);
      return results;
   }
   vector<digest_type> get_contests_by_tags(string key, string value)
   {
      vector<digest_type> results;
      auto range = contest_tags_index.get<by_tags>().equal_range(boost::make_tuple(key, value));
      if( value.empty() )
         range = contest_tags_index.get<by_tags>().equal_range(boost::make_tuple(key));
      std::for_each(range.first, range.second, [&results](const contest_tags_index_record& rec) {
         results.push_back(rec.contest_id);
      });
      return results;
   }
   vector<digest_type> get_contests_by_contestant(string contestant)
   {
      vector<digest_type> results;
      auto range = contest_by_contestant_index.equal_range(contestant);
      std::for_each(range.first, range.second, [&results](const std::pair<string, digest_type>& rec) {
         results.push_back(rec.second);
      });
      return results;
   }
};
} // namespace detail

ballot_box::ballot_box()
{
   my = std::make_shared<detail::ballot_box_impl>();
}

void ballot_box::open(fc::path data_dir)
{
   my->open(data_dir);
}

bool ballot_box::is_open()
{
   return my->databases_open;
}

void ballot_box::clear()
{
   my->clear();
}

void ballot_box::close()
{
   my->decision_db.close();
}

void ballot_box::store_new_decision(const signed_voter_decision& decision)
{
   my->store_record(decision);
}

cast_decision ballot_box::get_decision(const digest_type& id)
{
   return my->decision_db.fetch(id);
}

vector<digest_type> ballot_box::list_decisions(const digest_type& skip_to_id, uint32_t limit)
{
   return my->list_decisions(skip_to_id, limit);
}

vector<digest_type> ballot_box::get_decisions_by_voter(const blockchain::address& voter)
{
   return my->get_decisions_by_voter(voter);
}

vector<digest_type> ballot_box::get_decisions_by_contest(const digest_type& contest_id)
{
   return my->get_decisions_by_contest(contest_id);
}

vector<digest_type> ballot_box::get_decisions_by_ballot(const digest_type& ballot_id)
{
   return my->get_decisions_by_ballot(ballot_id);
}

vector<string> ballot_box::get_all_write_ins()
{
   return my->get_all_write_ins();
}

vector<digest_type> ballot_box::get_decisions_with_write_in(string write_in_name)
{
   return my->get_decisions_with_write_in(write_in_name);
}

ballot ballot_box::get_ballot(const digest_type& id)
{
   return my->ballot_db.fetch(id);
}

vector<digest_type> ballot_box::list_ballots(const digest_type& skip_to_id, uint32_t limit)
{
   return my->list_leveldb(my->ballot_db, skip_to_id, limit);
}

void ballot_box::store_ballot(const vote::ballot& ballot)
{
   my->store_record(ballot);
}

vector<digest_type> ballot_box::get_ballots_by_contest(const digest_type& contest_id)
{
   return my->get_ballots_by_contest(contest_id);
}

contest ballot_box::get_contest(const digest_type& id)
{
   return my->contest_db.fetch(id);
}

vector<digest_type> ballot_box::list_contests(const digest_type& skip_to_id, uint32_t limit)
{
   return my->list_leveldb(my->contest_db, skip_to_id, limit);
}

void ballot_box::store_contest(const vote::contest& contest)
{
   my->store_record(contest);
}

vector<string> ballot_box::get_values_by_tag(string key)
{
   return my->get_values_by_tag(key);
}

vector<digest_type> ballot_box::get_contests_by_tags(string key, string value)
{
   return my->get_contests_by_tags(key, value);
}

vector<digest_type> ballot_box::get_contests_by_contestant(string contestant)
{
   return my->get_contests_by_contestant(contestant);
}

} } // namespace bts::vote

FC_REFLECT_DERIVED( bts::vote::detail::ballot_box_impl::decision_storage_record,
                    (bts::vote::voter_decision),
                    (voter_key)(timestamp)(authoritative) )
