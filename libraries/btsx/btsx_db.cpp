#include <bts/btsx/btsx_db.hpp>

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
   }

   btsx_db::~btsx_db()
   {
   }

} } // bts::btsx
