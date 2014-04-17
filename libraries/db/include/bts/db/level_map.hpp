#pragma once
#include <leveldb/db.h>
#include <leveldb/comparator.h>

#include <fc/filesystem.hpp>

#include <fc/reflect/reflect.hpp>
#include <fc/io/raw.hpp>
#include <fc/exception/exception.hpp>

#include <fc/log/logger.hpp>

#include <bts/db/upgrade_leveldb.hpp>

namespace bts { namespace db {

  namespace ldb = leveldb;

  /**
   *  @brief implements a high-level API on top of Level DB that stores items using fc::raw / reflection
   *
   */
  template<typename Key, typename Value>
  class level_map
  {
     public:
        void open( const fc::path& dir, bool create = true )
        {
           ldb::Options opts;
           opts.create_if_missing = create;
           opts.comparator = & _comparer;

           /// \waring Given path must exist to succeed toNativeAnsiPath
           fc::create_directories(dir);

           std::string ldbPath = dir.to_native_ansi_path();

           ldb::DB* ndb = nullptr;
           auto ntrxstat = ldb::DB::Open( opts, ldbPath.c_str(), &ndb );
           if( !ntrxstat.ok() )
           {
               FC_THROW_EXCEPTION( db_in_use_exception, "Unable to open database ${db}\n\t${msg}",
                    ("db",dir)
                    ("msg",ntrxstat.ToString())
                    );
           }
           _db.reset(ndb);
           try_upgrade_db( dir,ndb, fc::get_typename<Value>::name(),sizeof(Value) );
        }

        void close()
        {
          _db.reset();
        }

        Value fetch( const Key& k )
        {
          try {
             std::vector<char> kslice = fc::raw::pack( k );
             ldb::Slice ks( kslice.data(), kslice.size() );
             std::string value;
             auto status = _db->Get( ldb::ReadOptions(), ks, &value );
             if( status.IsNotFound() )
             {
               FC_THROW_EXCEPTION( key_not_found_exception, "unable to find key ${key}", ("key",k) );
             }
             if( !status.ok() )
             {
                 FC_THROW_EXCEPTION( exception, "database error: ${msg}", ("msg", status.ToString() ) );
             }
             fc::datastream<const char*> ds(value.c_str(), value.size());
             Value tmp;
             fc::raw::unpack(ds, tmp);
             return tmp;
          } FC_RETHROW_EXCEPTIONS( warn, "error fetching key ${key}", ("key",k) );
        }

        class iterator
        {
           public:
             iterator(){}
             bool valid()const
             {
                return _it && _it->Valid();
             }

             Key key()const
             {
                 Key tmp_key;
                 fc::datastream<const char*> ds2( _it->key().data(), _it->key().size() );
                 fc::raw::unpack( ds2, tmp_key );
                 return tmp_key;
             }

             Value value()const
             {
               Value tmp_val;
               fc::datastream<const char*> ds( _it->value().data(), _it->value().size() );
               fc::raw::unpack( ds, tmp_val );
               return tmp_val;
             }

             iterator& operator++()    { _it->Next(); return *this; }
             iterator  operator++(int) { _it->Next(); return *this; }

             iterator& operator--()    { _it->Prev(); return *this; }
             iterator  operator--(int) { _it->Prev(); return *this; }

           protected:
             friend class level_map;
             iterator( ldb::Iterator* it )
             :_it(it){}

             std::shared_ptr<ldb::Iterator> _it;
        };

        iterator begin()
        { try {
           iterator itr( _db->NewIterator( ldb::ReadOptions() ) );
           itr._it->SeekToFirst();

           if( itr._it->status().IsNotFound() )
           {
             FC_THROW_EXCEPTION( key_not_found_exception, "" );
           }
           if( !itr._it->status().ok() )
           {
               FC_THROW_EXCEPTION( exception, "database error: ${msg}", ("msg", itr._it->status().ToString() ) );
           }

           if( itr.valid() )
           {
              return itr;
           }
           return iterator();
        } FC_RETHROW_EXCEPTIONS( warn, "error seeking to first" ) }

        iterator find( const Key& key )
        { try {
           std::vector<char> kslice = fc::raw::pack( key );
           ldb::Slice key_slice( kslice.data(), kslice.size() );
           iterator itr( _db->NewIterator( ldb::ReadOptions() ) );
           itr._it->Seek( key_slice );
           if( itr.valid() && itr.key() == key )
           {
              return itr;
           }
           return iterator();
        } FC_RETHROW_EXCEPTIONS( warn, "error finding ${key}", ("key",key) ) }

        iterator lower_bound( const Key& key )
        { try {
           ldb::Slice key_slice( (char*)&key, sizeof(key) );
           iterator itr( _db->NewIterator( ldb::ReadOptions() ) );
           itr._it->Seek( key_slice );
           if( itr.valid()  )
           {
              return itr;
           }
           return iterator();
        } FC_RETHROW_EXCEPTIONS( warn, "error finding ${key}", ("key",key) ) }

        bool last( Key& k )
        {
          try {
             std::unique_ptr<ldb::Iterator> it( _db->NewIterator( ldb::ReadOptions() ) );
             FC_ASSERT( it != nullptr );
             it->SeekToLast();
             if( !it->Valid() )
             {
               return false;
             }
             fc::datastream<const char*> ds2( it->key().data(), it->key().size() );
             fc::raw::unpack( ds2, k );
             return true;
          } FC_RETHROW_EXCEPTIONS( warn, "error reading last item from database" );
        }

        bool last( Key& k, Value& v )
        {
          try {
           std::unique_ptr<ldb::Iterator> it( _db->NewIterator( ldb::ReadOptions() ) );
           FC_ASSERT( it != nullptr );
           it->SeekToLast();
           if( !it->Valid() )
           {
             return false;
           }
           fc::datastream<const char*> ds( it->value().data(), it->value().size() );
           fc::raw::unpack( ds, v );

           fc::datastream<const char*> ds2( it->key().data(), it->key().size() );
           fc::raw::unpack( ds2, k );
           return true;
          } FC_RETHROW_EXCEPTIONS( warn, "error reading last item from database" );
        }

        void store( const Key& k, const Value& v )
        {
          try
          {
             FC_ASSERT( _db != nullptr );

             std::vector<char> kslice = fc::raw::pack( k );
             ldb::Slice ks( kslice.data(), kslice.size() );

             auto vec = fc::raw::pack(v);
             ldb::Slice vs( vec.data(), vec.size() );

             auto status = _db->Put( ldb::WriteOptions(), ks, vs );
             if( !status.ok() )
             {
                 FC_THROW_EXCEPTION( exception, "database error: ${msg}", ("msg", status.ToString() ) );
             }
          } FC_RETHROW_EXCEPTIONS( warn, "error storing ${key} = ${value}", ("key",k)("value",v) );
        }

        void remove( const Key& k )
        {
          try
          {
             FC_ASSERT( _db != nullptr );

             std::vector<char> kslice = fc::raw::pack( k );
             ldb::Slice ks( kslice.data(), kslice.size() );
             auto status = _db->Delete( ldb::WriteOptions(), ks );
             if( status.IsNotFound() )
             {
               FC_THROW_EXCEPTION( key_not_found_exception, "unable to find key ${key}", ("key",k) );
             }
             if( !status.ok() )
             {
                 FC_THROW_EXCEPTION( exception, "database error: ${msg}", ("msg", status.ToString() ) );
             }
          } FC_RETHROW_EXCEPTIONS( warn, "error removing ${key}", ("key",k) );
        }

     private:
        class key_compare : public leveldb::Comparator
        {
          public:
            int Compare( const leveldb::Slice& a, const leveldb::Slice& b )const
            {
               Key ak,bk;
               fc::datastream<const char*> dsa( a.data(), a.size() );
               fc::raw::unpack( dsa, ak );
               fc::datastream<const char*> dsb( b.data(), b.size() );
               fc::raw::unpack( dsb, bk );

               if( ak  < bk ) return -1;
               if( ak == bk ) return 0;
               return 1;
            }

            const char* Name()const { return "key_compare"; }
            void FindShortestSeparator( std::string*, const leveldb::Slice& )const{}
            void FindShortSuccessor( std::string* )const{};
        };

        key_compare                  _comparer;

public: //DLNFIX temporary, remove this
        std::unique_ptr<leveldb::DB> _db;
  };

} } // bts::db
