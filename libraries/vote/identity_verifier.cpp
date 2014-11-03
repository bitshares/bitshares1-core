#include <bts/vote/identity_verifier.hpp>
#include <bts/db/level_map.hpp>

#include <fc/io/raw_variant.hpp>
#include <fc/reflect/variant.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/ordered_index.hpp>

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
      request.owner_photo = person_photo;
      request.id_front_photo = id_card_front_photo;
      request.id_back_photo = id_card_back_photo;
      request.voter_reg_photo = voter_registration_photo;
      return request;
   }

   public_key_type owner_key;
   ///Photos are stored as base64 JPEGs
   string person_photo;
   string id_card_front_photo;
   string id_card_back_photo;
   string voter_registration_photo;
};

class identity_verifier_impl {
protected:
   //Dummy types to tag multi_index_container indices
   struct by_owner{};
   struct by_status{};
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
      if( _id_summary_db.get<by_owner>().find(rec.owner) != _id_summary_db.get<by_owner>().end() )
         //If this owner has already started the verification process, just return the status.
         return check_pending_identity(rec.owner);

      _id_summary_db.insert(rec);
      _id_db.store(rec.id, rec);

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
         response.accepted = false;
         response.rejection_reason = record.rejection_reason;
         return response;
      }
      response.accepted = true;
      response.verified_identity = record;
      return response;
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

      while( itr != end && results.size() < limit )
         results.push_back(*itr++);

      return results;
   }

   void commit_record(const identity_record& record)
   {
      _id_summary_db.insert(record);
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

   optional<identity_verification_request> take_next_request()
   {
      auto request_summary_vector = list_requests_by_status(awaiting_processing, fc::microseconds(), 1);
      if( request_summary_vector.empty() )
         return optional<identity_verification_request>();

      return update_record_status(request_summary_vector[0].id, in_processing);
   }
   void resolve_request(fc::microseconds id, identity_verification_response response)
   {
      //We need to further update the record; don't bother to commit it just yet. We'll do it just once at the end.
      identity_record request_record = update_record_status(id, response.accepted? accepted : rejected, false);
      if( response.accepted )
      {
         FC_ASSERT( response.verified_identity, "Identity accepted, but verified_identity was not provided." );
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
         request_record.properties = std::move(response.verified_identity->properties);
      } else {
         //Request was rejected. Remove all properties, they must not be signed.
         request_record.properties.clear();
      }
      //Copy over the rejection reason even if the request was accepted. No harm in passing it along if the verifier
      //saw fit to set it.
      request_record.rejection_reason = response.rejection_reason;
      commit_record(request_record);
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

vector<identity_verification_request_summary> identity_verifier::list_pending_requests(fc::microseconds after_time,
                                                                                       uint32_t limit) const
{
   SANITY_CHECK;

   return my->list_requests_by_status(awaiting_processing, after_time, limit);
}

identity_verification_request identity_verifier::peek_pending_request(fc::microseconds request_id) const
{
   SANITY_CHECK;

   return my->fetch_request_by_id(request_id);
}

fc::optional<identity_verification_request> identity_verifier::take_next_request()
{
   SANITY_CHECK;

   return my->take_next_request();
}

void identity_verifier::resolve_request(fc::microseconds request_id, const identity_verification_response& response)
{
   SANITY_CHECK;

   my->resolve_request(request_id, response);
}

} } // namespace bts::vote

FC_REFLECT_DERIVED( bts::vote::detail::identity_record, (bts::vote::identity_verification_request_summary),
                    (owner_key)(person_photo)(id_card_front_photo)(id_card_back_photo)(voter_registration_photo) )
