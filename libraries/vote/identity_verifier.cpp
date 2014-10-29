#include <bts/vote/identity_verifier.hpp>
#include <bts/db/level_map.hpp>

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

enum status_enum {
   awaiting_processing,
   in_processing,
   accepted,
   rejected
};

struct identity_record_summary : public vote::identity {
   identity_record_summary()
       :timestamp(fc::time_point::now())
   {}

   bool operator< (const identity_record_summary& other) const {
      if( status != other.status ) return status < other.status;
      return timestamp < other.timestamp;
   }
   bool operator== (const identity_record_summary& other) const {
      return status == other.status && timestamp == other.timestamp;
   }

   status_enum status = awaiting_processing;
   fc::time_point timestamp;
   fc::optional<string> rejection_reason;
};

struct identity_record : public identity_record_summary {
   identity_record(){}
   identity_record(const identity_verification_request &request, const fc::ecc::compact_signature& signature)
       : owner_key(fc::ecc::public_key(signature, request.digest())),
         person_photo(request.owner_photo),
         id_card_front_photo(request.id_front_photo),
         id_card_back_photo(request.id_back_photo),
         voter_registration_photo(request.voter_reg_photo)
   {
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
      identity_record_summary,
      indexed_by<
         ordered_unique<
            tag<by_status>,
            boost::multi_index::identity<identity_record_summary>
         >,
         //Allow constant time lookups by owner address
         hashed_unique<
            tag<by_owner>,
            member<vote::identity, bts::blockchain::address, &identity::owner>,
            std::hash<bts::blockchain::address>
         >
      >
   > _id_summary_db;
   bts::db::level_map<fc::time_point, identity_record> _id_db;

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
            rec.status = awaiting_processing;
         _id_summary_db.insert(rec);
      }
   }
   void close()
   {
      if( is_open() )
         _id_db.close();
   }
   bool is_open()
   {
      return _id_db.is_open();
   }

   optional<identity_verification_response> store_new_request(identity_record&& rec)
   {
      if( _id_summary_db.get<by_owner>().find(rec.owner) != _id_summary_db.get<by_owner>().end() )
         //If this owner has already started the verification process, just return the status.
         return check_pending_identity(rec.owner);

      _id_summary_db.insert(rec);
      _id_db.store(rec.timestamp, rec);

      return optional<identity_verification_response>();
   }
   optional<identity_verification_response> check_pending_identity(const address& owner) const
   {
      identity_record_summary record;
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

bool identity_verifier::is_open()
{
   return my->is_open();
}

fc::optional<identity_verification_response> identity_verifier::store_new_request(
        const identity_verification_request& request,
        const fc::ecc::compact_signature& signature)
{
   SANITY_CHECK;

   return my->store_new_request(detail::identity_record(request, signature));
}
fc::optional<identity_verification_response> identity_verifier::get_verified_identity(const blockchain::address& owner)
{
   SANITY_CHECK;

   return my->check_pending_identity(owner);
}

} } // namespace bts::vote

FC_REFLECT_TYPENAME( bts::vote::detail::status_enum )
FC_REFLECT_ENUM( bts::vote::detail::status_enum,
                 (awaiting_processing)(in_processing)(accepted)(rejected) )
FC_REFLECT_DERIVED( bts::vote::detail::identity_record_summary, (bts::vote::identity),
                    (status)(timestamp)(rejection_reason) )
FC_REFLECT_DERIVED( bts::vote::detail::identity_record, (bts::vote::detail::identity_record_summary),
                    (owner_key)(person_photo)(id_card_front_photo)(id_card_back_photo)(voter_registration_photo) )
