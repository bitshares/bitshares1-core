#pragma once
#include <bts/db/level_map.hpp>
#include <map>
#include <fc/exception/exception.hpp>
#include <fc/thread/thread.hpp>

namespace bts { namespace db {

   template<typename Key, typename Value, class CacheType = std::map<Key,Value> >
   class cached_level_map 
   {
      public:
         void open( const fc::path& dir, bool create = true, bool flush_on_store = true )
         {
           _flush_on_store = flush_on_store;
           _db.open( dir, create ); 
           for( auto itr = _db.begin(); itr.valid(); ++itr )
              _cache[itr.key()]  = itr.value();
         }

         void close()
         {
            if(_pending_flush.valid() )
               _pending_flush.wait();
            else
               flush();
            _db.close();
         }

         bool get_flush_on_store()
         {
            return _flush_on_store;
         }
         void set_flush_on_store( bool should_flush )
         {
            if( should_flush )
               flush();
            _flush_on_store = should_flush;
         }

         void flush()
         {
            // TODO... 
            // start batch
            for( auto item : _dirty )
               _db.store( item, _cache[item] );
            for( auto item : _dirty_remove )
               _db.remove( item );
            // end batch
            _dirty.clear();
            _dirty_remove.clear();
         }

        fc::optional<Value> fetch_optional( const Key& k )
        {
           auto itr = _cache.find(k);
           if( itr != _cache.end() ) return itr->second;
           return fc::optional<Value>();
        }

        Value fetch( const Key& key ) const
        { try {
           auto itr = _cache.find(key);
           if( itr != _cache.end() ) return itr->second;
           FC_CAPTURE_AND_THROW( fc::key_not_found_exception, (key) );
        } FC_CAPTURE_AND_RETHROW( (key) ) }

        void store( const Key& key, const Value& value )
        { try {
             _cache[key] = value;
             if( _flush_on_store )
             {
                 _dirty.insert(key);
                 _dirty_remove.erase(key);
                 if( !_pending_flush.valid() || _pending_flush.ready() )
                    _pending_flush = fc::async( [this](){ flush(); }, "cached_level_map::flush" );
                _db.store( key, value );
             } else {
                 _dirty.insert(key);
                 _dirty_remove.erase(key);
             }
        } FC_CAPTURE_AND_RETHROW( (key)(value) ) }

        bool last( Key& k )
        {
           auto ritr = _cache.rbegin();
           if( ritr != _cache.rend() )
           {
              k = ritr->first;
              return true;
           }
           return false;
        }
        bool last( Key& k, Value& v )
        {
           flush();
           return _db.last( k, v );
        }

        void remove( const Key& key )
        { try {
           _cache.erase(key);
           if( _flush_on_store )
           {
              _db.remove(key);
              _dirty.erase(key);
           } else {
              _dirty_remove.insert(key);
           }
        } FC_CAPTURE_AND_RETHROW( (key) ) }

        class iterator
        {
           public:
             iterator(){}
             bool valid()const { return _it != _end; }

             Key   key()const { return _it->first; }
             Value value()const { return _it->second; }

             iterator& operator++()    { ++_it; return *this; }
             iterator  operator++(int) {
                auto backup = *this;
                ++_it;
                return backup;
             }

             iterator& operator--()
             {
                if( _it == _begin )
                   _it = _end;
                else
                   --_it;
                return *this;
             }
             iterator  operator--(int) {
                auto backup = *this;
                operator--();
                return backup;
             }

           protected:
             friend class cached_level_map;
             iterator( typename CacheType::const_iterator it, typename CacheType::const_iterator begin, typename CacheType::const_iterator end )
             :_it(it),_begin(begin),_end(end)
             { }

             typename CacheType::const_iterator _it;
             typename CacheType::const_iterator _begin;
             typename CacheType::const_iterator _end;
        };
        iterator begin()const
        {
           return iterator( _cache.begin(), _cache.begin(), _cache.end() );
        }
        iterator last()
        {
           if( _cache.empty() )
              return iterator( _cache.end(), _cache.begin(), _cache.end() );
           return iterator( --_cache.end(), _cache.begin(), _cache.end() );
        }

        iterator find( const Key& key )
        {
           return iterator( _cache.find(key), _cache.begin(), _cache.end() );
        }
        iterator lower_bound( const Key& key )
        {
           return iterator( _cache.lower_bound(key), _cache.begin(), _cache.end() );
        }

      private:
        CacheType                _cache;
        std::set<Key>            _dirty;
        std::set<Key>            _dirty_remove;
        level_map<Key,Value>     _db;
        bool                     _flush_on_store;
        fc::future<void>         _pending_flush;
   };

} }
