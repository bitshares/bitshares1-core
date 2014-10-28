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

struct identity_record : public vote::identity {
   enum status_enum {
      awaiting_processing,
      in_processing,
      accepted,
      rejected
   };

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

   status_enum status = awaiting_processing;
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
      identity_record,
      indexed_by<
         //Allow constant time lookups by owner address
         hashed_unique<
            tag<by_owner>,
            member<identity, bts::blockchain::address, &identity::owner>,
            std::hash<bts::blockchain::address>
         >,
         //Allow retrieval by accepted status
         ordered_non_unique<
            tag<by_status>,
            member<identity_record, identity_record::status_enum, &identity_record::status>
         >
      >
   > _processing_id_db;
   bts::db::level_map<address, identity_record> _id_db;

   void open(const fc::path& data_dir)
   {
      FC_ASSERT( !is_open(), "Refusing to open already-open ID verifier." );

      _id_db.open(data_dir / "id_db");
      //Load unprocessed records into RAM; we'll need these soon.
      for( auto itr = _id_db.begin();
           itr.valid() && itr.value().status <= identity_record::in_processing;
           ++itr )
      {
         auto rec = itr.value();
         //This is a startup routine; if there are in_processing records on disk, we probably crashed.
         //Mark in_processing records as awaiting_processing again; no telling what might have happened
         //in the real world since it was marked as in_processing
         rec.status = identity_record::awaiting_processing;
         _processing_id_db.insert(rec);
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

   void store_pending_identity(identity_record&& rec)
   {
      _processing_id_db.insert(rec);
      _id_db.store(rec.owner, rec);
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

void identity_verifier::verify_identity(const identity_verification_request& request,
                                        const fc::ecc::compact_signature& signature)
{
   SANITY_CHECK;

   my->store_pending_identity(detail::identity_record(request, signature));
}

} } // namespace bts::vote

FC_REFLECT_TYPENAME( bts::vote::detail::identity_record::status_enum )
FC_REFLECT_ENUM( bts::vote::detail::identity_record::status_enum,
                 (awaiting_processing)(in_processing)(accepted)(rejected) )
FC_REFLECT_DERIVED( bts::vote::detail::identity_record, (bts::vote::identity),
                    (status)(person_photo)(id_card_front_photo)(id_card_back_photo)(voter_registration_photo) )
