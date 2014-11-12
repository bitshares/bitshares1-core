#include <bts/vote/ballot_box.hpp>
#include <bts/db/level_map.hpp>

#include <fc/reflect/variant.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
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
public:
   struct decision_storage_record : public signed_voter_decision
   {
      decision_storage_record(const signed_voter_decision& = signed_voter_decision()){}

      bts::blockchain::public_key_type voter_key = voter_public_key();
   };
   struct decision_index_record
   {
      decision_index_record(const decision_storage_record& s = decision_storage_record())
         : decision_index_record(s.digest(), s){}
      decision_index_record(const digest_type& id, const decision_storage_record& s = decision_storage_record())
         : id(id),
           voter(s.voter_key),
           contest_id(s.contest_id),
           write_in_names(s.write_in_names),
           ballot_id(s.ballot_id)
      {}

      digest_type id;
      address voter;
      digest_type contest_id;
      vector<string> write_in_names;
      digest_type ballot_id;
   };

   level_map<digest_type, signed_voter_decision> decision_db;
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
         >
      >
   > decision_index;
   std::multimap<string, digest_type> write_in_index;

   bool databases_open = false;

   void update_index(const digest_type& id, const decision_storage_record& storage_record)
   {
      decision_index.insert(decision_index_record(id, storage_record));
      for( auto write_in : storage_record.write_in_names )
         write_in_index.insert(std::make_pair(write_in, id));
   }

   void store_record(const signed_voter_decision& decision)
   {
      auto id = decision.digest();
      decision_storage_record record = decision;
      decision_db.store(id, record);
      update_index(id, record);
   }

   void open(fc::path data_dir)
   {
      FC_ASSERT( !databases_open, "Refusing to open already-opened ballot box." );

      decision_db.open(data_dir / "decision_db");
      for( auto itr = decision_db.begin(); itr.valid(); ++itr )
         update_index(itr.key(), itr.value());

      databases_open = true;
   }
   void close()
   {
      databases_open = false;

      decision_db.close();
      decision_index.clear();
      write_in_index.clear();
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
      for( auto itr = write_in_index.lower_bound(write_in_name);
           itr != write_in_index.upper_bound(write_in_name);
           ++itr )
         results.push_back(itr->second);
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

void ballot_box::close()
{
   my->decision_db.close();
}

void ballot_box::store_new_decision(const signed_voter_decision& decision)
{
   my->store_record(decision);
}

signed_voter_decision ballot_box::get_decision(const digest_type& id)
{
   return my->decision_db.fetch(id);
}

vector<string> ballot_box::get_all_write_ins()
{
   return my->get_all_write_ins();
}

vector<digest_type> ballot_box::get_decisions_with_write_in(string write_in_name)
{
   return my->get_decisions_with_write_in(write_in_name);
}

} } // namespace bts::vote

FC_REFLECT_DERIVED( bts::vote::detail::ballot_box_impl::decision_storage_record,
                    (bts::vote::voter_decision),
                    (voter_key) )
