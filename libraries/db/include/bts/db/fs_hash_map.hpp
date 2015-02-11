#include <fc/io/fstream.hpp>
#include <fc/interprocess/file_mapping.hpp>
#include <fc/io/raw.hpp>

#include <sstream>

namespace bts { namespace db {

   /**
    *  This class uses the filesystem to map key/value pairs under the
    *  assumption that storing and 
    *  Key must be convertable to a string
    */
   template<typename Key, typename Value>
   class fs_hash_map
   {
      public:
         void open( const fc::path& root_dir, bool create = true, size_t cache_size = 0 )
         {
            _root_dir = root_dir;
         }
         bool is_open()const { return _root_dir != fc::path(); }
         void close()        { _root_dir = fc::path();         }

         optional<Value> fetch_optional( const Key& k )
         { try {
            auto key_path = key_to_path(k);
            if( fc::exists( key_to_path ) )
            {
               // memmap, read, return value
               fc::file_mapping fm( key_to_path.c_str(), fc::read_only );
               fc::mapped_region mr( fm, fc::read_only );
               const char* addr = (const char*)mr.get_address();
               size_t s   = mr.get_size();
               datastream<const char*> ds(addr,s);
               Value v;
               fc::raw::unpack( ds, v );
               return v;
            }
            return optional<Value>();
         } FC_CAPTURE_AND_RETHROW( (k) ) }

         Value fetch( const Key& k )
         { try {
            auto opt_val = fetch_optional(k);
            FC_ASSERT( opt_val.valid() );
            return *opt_val;
         } FC_CAPTURE_AND_RETHROW( (k) ) }

         void store( const Key& k, const Value& v )
         { try {
            auto key_path = key_to_path(k);
            fc::create_directories( key_path.parent_path() );
            auto data = fc::raw::pack( v );
            FC_ASSERT( data.size() > 0  );
            
            ofstream out( key_path );
            out.writesome( data.data(), data.size() );
            out.close();
         } FC_CAPTURE_AND_RETHROW( (k)(v) ) }
         
         void remove( const Key& k )
         { try {
            fc::remove( key_to_path(k) );
         } FC_CAPTURE_AND_RETHROW( (k) ) }

      private:
         fc::path key_to_path( const Key& k )
         {
            FC_ASSERT( is_open() );
            auto str = string(k);
            FC_ASSERT( str.size() >= 10 );
            std::stringstream ss;
            ss << _root_dir.generic_string() << "/";
            ss << str.substr( 0, 2 ) << "/";
            ss << str.substr( 2, 2 ) << "/";
            ss << str.substr( 4, 3 ) << "/";
            ss << str.substr( 7 ) << ".dat";
            return fc::path(ss.str()); 
         }

         fc::path _root_dir;
   };

} }
