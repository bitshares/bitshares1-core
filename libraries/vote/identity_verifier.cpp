#include <bts/vote/identity_verifier.hpp>
#include <bts/db/level_map.hpp>

#include <fc/io/raw_variant.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <iostream>

#define SANITY_CHECK\
    FC_ASSERT( is_open(), "ID Verifier is not open. Is the ID verifier server enabled in the config?" )

namespace bts { namespace vote {
namespace detail {
using namespace boost::multi_index;

struct identity_record : public identity_verification_request_summary {
   identity_record(){}
   identity_record(const identity_verification_request &request, const public_key_type& owner_key)
       : owner_key(owner_key),
         person_photo(request.owner_photo),
         id_card_front_photo(request.id_front_photo),
         id_card_back_photo(request.id_back_photo),
         voter_registration_photo(request.voter_reg_photo)
   {
      (identity&)*this = request;
      owner = owner_key;
   }

   operator identity_verification_request()
   {
      identity_verification_request request;
      (identity&)(request) = *this;
      //Copy relevant fields from record to request.
      std::tie(request.owner_photo, request.id_front_photo, request.id_back_photo, request.voter_reg_photo, request.id) =
            std::tie(person_photo, id_card_front_photo, id_card_back_photo, voter_registration_photo, id);
      return request;
   }

   public_key_type owner_key;
   ///Photos are stored as base64 JPEGs
   string person_photo;
   string id_card_front_photo;
   string id_card_back_photo;
   string voter_registration_photo;

   fc::optional<identity_verification_response> response;

   vector<fc::microseconds> conflicting_ids;
};
struct identity_uniqueness_record {
   string id_number;
   string name;
   string date_of_birth;
   fc::microseconds id;
};

class identity_verifier_impl {
protected:
   //Dummy types to tag multi_index_container indices
   struct by_owner;
   struct by_status;
   struct by_id;
   struct by_name_dob;
public:
   boost::multi_index_container<
      identity_verification_request_summary,
      indexed_by<
         //Allow constant time lookups by owner address
         hashed_unique<
            tag<by_owner>,
            member<vote::identity, bts::blockchain::address, &identity::owner>,
            std::hash<bts::blockchain::address>
         >,
         ordered_unique<
            tag<by_status>,
            composite_key<
               identity_verification_request_summary,
               member<identity_verification_request_summary,
                      request_status_enum,
                      &identity_verification_request_summary::status>,
               member<identity_verification_request_summary,
                      fc::microseconds,
                      &identity_verification_request_summary::id>
            >
         >
      >
   > _id_summary_db;
   boost::multi_index_container<
      identity_uniqueness_record,
      indexed_by<
         hashed_unique<
            tag<by_id>,
            member<identity_uniqueness_record, string, &identity_uniqueness_record::id_number>
         >,
         hashed_non_unique<
            tag<by_name_dob>,
            composite_key<
               identity_uniqueness_record,
               member<identity_uniqueness_record, string, &identity_uniqueness_record::name>,
               member<identity_uniqueness_record, string, &identity_uniqueness_record::date_of_birth>
            >
         >
      >
   > _id_unique_db;
   bts::db::level_map<fc::microseconds, identity_record> _id_db;

   void open(const fc::path& data_dir)
   {
      FC_ASSERT( !is_open(), "Refusing to open already-open ID verifier." );

      _id_db.open(data_dir / "id_db");
      //Load unprocessed records into RAM; we'll need these soon.
      for( auto itr = _id_db.begin(); itr.valid(); ++itr )
      {
         auto rec = itr.value();
         //This is a startup routine; if there are in_processing records on disk, we probably crashed.
         //Mark in_processing records as awaiting_processing again; no telling what might have happened
         //in the real world since it was marked as in_processing
         if( rec.status == in_processing )
            update_record_status(rec.id, awaiting_processing);
         else
            _id_summary_db.insert(rec);

         //Put all accepted records through post-processing again, to populate the uniqueness DB
         if( rec.status == accepted )
         {
            postprocess_accepted_record(rec, true);
            if( rec.status != accepted ) {
               elog("CRITICAL: Record from verifier database was marked as accepted but failed post-processing: ${r}",
                    ("r", rec));
               std::cerr << "CRITICAL: Record from verifier database was marked as accepted but failed post-processing:\n"
                    << fc::json::to_pretty_string(rec);
            }
         }
      }
   }
   void close()
   {
      if( is_open() )
         _id_db.close();
   }
   bool is_open() const
   {
      return _id_db.is_open();
   }

   optional<identity_verification_response> store_new_request(identity_record rec)
   {
      auto itr = _id_summary_db.get<by_owner>().find(rec.owner);
      if( itr != _id_summary_db.get<by_owner>().end() && itr->status == accepted )
      {
         //If this owner has already completed the verification process, just return the status.
         return check_pending_identity(rec.owner);
      }

      //Only record the new request if it's at least 30 seconds older than the old one
      if( rec.id - itr->id >= fc::seconds(30) )
         commit_record(rec);

      return optional<identity_verification_response>();
   }
   identity_record fetch_request_by_id(fc::microseconds id) const
   {
      return _id_db.fetch(id);
   }
   optional<identity_verification_response> check_pending_identity(const address& owner) const
   {
      identity_verification_request_summary record;
      auto& index = _id_summary_db.get<by_owner>();
      auto itr = index.find(owner);

      if( itr != index.end() )
         //Usually the ID we're after will be in the recent database.
         record = *itr;
      else
         FC_ASSERT( false, "This identity has never been submitted for verification." );

      if( record.status <= in_processing )
         //Record has not yet been processed. Return null.
         return optional<identity_verification_response>();
      identity_verification_response response;
      if( record.status != accepted )
      {
         //Record has been processed and rejected. Return a false acceptance
         //Don't bother returning an identity; we didn't fill it out anyways
         // TODO: Do we ever do partial acceptances? If so, we should return an identity after all.
         response = *_id_db.fetch(record.id).response;
         response.accepted = false;
         response.verified_identity = fc::optional<identity>();
         response.rejection_reason = record.rejection_reason;
         return response;
      }
      auto disk_record = _id_db.fetch(record.id);
      FC_ASSERT( disk_record.response, "Internal error: request was accepted, but response was not stored." );
      return *disk_record.response;
   }

   vector<identity_verification_request_summary> list_requests_by_status(request_status_enum status,
                                                                         fc::microseconds after_time,
                                                                         uint32_t limit) const
   {
      vector<identity_verification_request_summary> results;
      auto& index = _id_summary_db.get<by_status>();
      auto end = index.end();
      auto itr = index.lower_bound(boost::make_tuple(status, after_time));

      if( limit == 0 || itr == end || itr->status != status )
         return results;
      if( itr->id == after_time ) ++itr;

      while( itr != end && results.size() < limit && itr->status == status )
         results.push_back(*itr++);

      return results;
   }

   void commit_record(const identity_record& record)
   {
      auto& index = _id_summary_db.get<by_owner>();
      auto itr = index.find(record.owner);
      if( itr == index.end() )
         _id_summary_db.insert(record);
      else
         _id_summary_db.replace(itr, record);

      _id_db.store(record.id, record);
   }
   identity_record update_record_status(fc::microseconds id, request_status_enum status, bool commit = true)
   {
      //Fetch record from disk, set its status, and possibly commit it as well.
      auto request = fetch_request_by_id(id);
      request.status = status;
      if( commit ) commit_record(request);
      return request;
   }

   optional<identity_verification_request> take_next_request(fc::microseconds request_id = fc::microseconds())
   {
      auto request_summary_vector = list_requests_by_status(awaiting_processing, request_id, 1);
      if( request_summary_vector.empty() )
         return optional<identity_verification_request>();

      return update_record_status(request_summary_vector[0].id, in_processing);
   }
   void resolve_request(fc::microseconds id, identity_verification_response response, bool skip_soft_checks)
   {
      //We need to further update the record; don't commit it yet. We'll do it just once at the end. If we fail midway
      //through processing a record, we don't want to have commited it.
      identity_record request_record = update_record_status(id, response.accepted? accepted : rejected, false);
      if( response.accepted )
      {
         FC_ASSERT( response.verified_identity, "Identity accepted, but verified_identity was not provided." );
         FC_ASSERT( response.expiration_date, "Identity accepted, but expiration date was not provided." );
         //For each verified property, copy the salt from the request to the verified property
         for( signed_identity_property& property : response.verified_identity->properties )
         {
            auto oproperty = request_record.get_property(property.name);
            FC_ASSERT( oproperty, "verified_identity contained property ${name} not in original request.",
                       ("name", property.name) );
            property.salt = request_record.get_property(property.name)->salt;
         }
         //Overwrite the request's properties with the now-salted approved properties. We don't want to keep any
         //properties the verifier did not approve of, as we will later sign all properties in the record.
         request_record.properties = response.verified_identity->properties;
      } else {
         //Request was rejected. Remove all properties, they must not be signed.
         request_record.properties.clear();
         response.verified_identity = decltype(response.verified_identity)();
      }
      //Copy over the rejection reason even if the request was accepted. No harm in passing it along if the verifier
      //saw fit to set it.
      request_record.response = response;
      request_record.rejection_reason = response.rejection_reason;

      if( request_record.status == accepted )
         postprocess_accepted_record(request_record, skip_soft_checks);

      commit_record(request_record);
   }

   bool soft_uniqueness_checks(const identity_uniqueness_record& unique_record, identity_record& record)
   {
      bool passes = true;
      record.conflicting_ids.clear();

      auto& name_dob_index = _id_unique_db.get<by_name_dob>();
      auto range = name_dob_index.equal_range(boost::make_tuple(unique_record.name, unique_record.date_of_birth));
      if( range.first != range.second )
      {
         passes = false;
         std::for_each(range.first, range.second, [&record](const identity_uniqueness_record& rec) {
            record.conflicting_ids.push_back(rec.id);
         });
      }

      return passes;
   }

   //Insert into uniqueness database. Mark for further review if a soft uniqueness contraint fails, or reject if a hard
   //uniqueness contraint fails.
   void postprocess_accepted_record(identity_record& record, bool skip_soft_checks)
   {
      auto first_name = record.get_property("First Name");
      auto middle_name = record.get_property("Middle Name");
      auto last_name = record.get_property("Last Name");
      auto dob = record.get_property("Date of Birth");
      auto id_number = record.get_property("ID Number");

      if( !(first_name && middle_name && last_name && dob && id_number) )
      {
         record.status = needs_further_review;
         record.rejection_reason = ("A required field was not provided. Required fields are all name fields, date of "
                                    "birth, and ID Number. Complete these fields and resubmit as an accepted identity.");
         return;
      }

      //Concatenate first, middle and last names; remove all non-alphabetical characters, and make completely upper-case
      string normalized_name = first_name->value.as_string()
            + middle_name->value.as_string()
            + last_name->value.as_string();
      normalized_name.erase(std::remove_if(normalized_name.begin(), normalized_name.end(), [](char c) {
         return !isalpha(c);
      }), normalized_name.end());
      for( char& c : normalized_name )
         c = toupper(c);

      identity_uniqueness_record unique_record = {id_number->value.as_string(),
                                                  normalized_name,
                                                  dob->value.as_string()};

      if( !skip_soft_checks )
      {
         bool soft_checks_passed = soft_uniqueness_checks(unique_record, record);
         if( !soft_checks_passed )
         {
            record.status = needs_further_review;
            record.rejection_reason = ("One or more soft-uniqueness checks failed. Please review all conflicting "
                                       "records and resubmit if the identity should be accepted anyways.");
            return;
         }
      }

      //Hard uniqueness checks are enforced by the container. Attempt insertion, and reject if it fails.
      if( !_id_unique_db.insert(unique_record).second )
      {
         record.status = rejected;
         record.rejection_reason = ("This identity has already been verified, and cannot be verified again.");
         return;
      }
   }
};
} // namespace detail

identity_verifier::identity_verifier()
    : my(new detail::identity_verifier_impl)
{}

identity_verifier::~identity_verifier()
{
   my->close();
}

void identity_verifier::open(fc::path data_dir)
{
   my->open(data_dir);
}

void identity_verifier::close()
{
   my->close();
}

bool identity_verifier::is_open() const
{
   return my->is_open();
}

fc::optional<identity_verification_response> identity_verifier::store_new_request(
        const identity_verification_request& request,
        const public_key_type& owner_key)
{
   SANITY_CHECK;
   return my->store_new_request(detail::identity_record(request, owner_key));
}
fc::optional<identity_verification_response> identity_verifier::get_verified_identity(const blockchain::address& owner) const
{
   SANITY_CHECK;
   return my->check_pending_identity(owner);
}

vector<identity_verification_request_summary> identity_verifier::list_requests(request_status_enum status,
                                                                               fc::microseconds after_time,
                                                                               uint32_t limit) const
{
   SANITY_CHECK;
   return my->list_requests_by_status(status, after_time, limit);
}

identity_verification_request identity_verifier::peek_request(fc::microseconds request_id) const
{
   SANITY_CHECK;
   return my->fetch_request_by_id(request_id);
}

identity_verification_request identity_verifier::take_pending_request(fc::microseconds request_id)
{
   SANITY_CHECK;
   auto request = my->take_next_request(request_id);
   FC_ASSERT( request, "The specified request does not exist or is not awaiting_processing." );
   return *request;
}

fc::optional<identity_verification_request> identity_verifier::take_next_request()
{
   SANITY_CHECK;
   return my->take_next_request();
}

void identity_verifier::resolve_request(fc::microseconds request_id, const identity_verification_response& response,
                                        bool skip_soft_checks)
{
   SANITY_CHECK;
   my->resolve_request(request_id, response, skip_soft_checks);
}

fc::variant identity_verifier::fetch_record(fc::microseconds record_id) const
{
   SANITY_CHECK;
   return fc::variant(my->fetch_request_by_id(record_id));
}

} } // namespace bts::vote

FC_REFLECT_DERIVED( bts::vote::detail::identity_record, (bts::vote::identity_verification_request_summary),
                    (owner_key)(person_photo)(id_card_front_photo)(id_card_back_photo)(voter_registration_photo)
                    (response)(conflicting_ids) )
