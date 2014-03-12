#include <bts/btsx/btsx_db.hpp>
#include <bts/btsx/btsx_transaction_validator.hpp>

namespace bts { namespace btsx {

   namespace detail
   {
      class btsx_db_impl
      {
         public:

      };

   } // namespace detail

   btsx_db::btsx_db()
   :my( new detail::btsx_db_impl() )
   {
       set_transaction_validator( std::make_shared<btsx_transaction_validator>(this) );
   }

   btsx_db::~btsx_db()
   {
   }

   void  btsx_db::open( const fc::path& dir, bool create )
   { try {
       chain_database::open( dir, create );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void  btsx_db::close()
   {
       chain_database::close();
   }

} } // bts::btsx
