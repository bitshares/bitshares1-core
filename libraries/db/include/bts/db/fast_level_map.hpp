#pragma once
#include <bts/db/level_map.hpp>

namespace bts { namespace db {

template<typename K, typename V>
class fast_level_map
{
    level_map<K, V>             _ldb;
    std::unordered_map<K, V>    _cache;

    bool                        _cache_write_through = true;
    std::unordered_set<K>       _cache_dirty_store;
    std::unordered_set<K>       _cache_dirty_remove;

public:

    void open( const fc::path& path )
    { try {
        _ldb.open( path );
        for( auto iter = _ldb.begin(); iter.valid(); ++iter )
            _cache.emplace( iter.key(), iter.value() );
    } FC_CAPTURE_AND_RETHROW( (path) ) }

    void close()
    { try {
        flush();
        _cache.clear();
        _ldb.close();
    } FC_CAPTURE_AND_RETHROW() }

    void set_write_through( bool write_through )
    { try {
        if( write_through == _cache_write_through )
            return;

        if( write_through )
            flush();

        _cache_write_through = write_through;
    } FC_CAPTURE_AND_RETHROW( (write_through) ) }

    void flush()
    { try {
        if( _cache_dirty_store.empty() && _cache_dirty_remove.empty() )
            return;

        typename level_map<K, V>::write_batch batch = _ldb.create_batch();

        if( !_cache_dirty_store.empty() )
        {
            for( const auto& key : _cache_dirty_store )
            {
                batch.store( key, _cache.at( key ) );
            }
            _cache_dirty_store.clear();
        }

        if( !_cache_dirty_remove.empty() )
        {
            for( const auto& key : _cache_dirty_remove )
            {
                batch.remove( key );
            }
            _cache_dirty_remove.clear();
        }

        batch.commit();
    } FC_CAPTURE_AND_RETHROW() }

    void store( const K& key, const V& value )
    { try {
        _cache[ key ] = value;

        if( _cache_write_through )
        {
            _ldb.store( key, value );
        }
        else
        {
            _cache_dirty_remove.erase( key );
            _cache_dirty_store.insert( key );
        }
    } FC_CAPTURE_AND_RETHROW( (key)(value) ) }

    void remove( const K& key )
    { try {
        _cache.erase( key );

        if( _cache_write_through )
        {
            _ldb.remove( key );
        }
        else
        {
            _cache_dirty_store.erase( key );
            _cache_dirty_remove.insert( key );
        }
    } FC_CAPTURE_AND_RETHROW( (key) ) }

    auto empty()const -> decltype( _cache.empty() )
    {
        return _cache.empty();
    }

    auto size()const -> decltype( _cache.size() )
    {
        return _cache.size();
    }

    auto unordered_begin()const -> decltype( _cache.cbegin() )
    {
        return _cache.cbegin();
    }

    auto unordered_end()const -> decltype( _cache.cend() )
    {
        return _cache.cend();
    }

    auto unordered_find( const K& key )const -> decltype( _cache.find( key ) )
    {
        return _cache.find( key );
    }

    auto ordered_first()const -> decltype( _ldb.begin() )
    { try {
        return _ldb.begin();
    } FC_CAPTURE_AND_RETHROW() }

    auto ordered_last()const -> decltype( _ldb.last() )
    { try {
        return _ldb.last();
    } FC_CAPTURE_AND_RETHROW() }

    auto ordered_lower_bound( const K& key )const -> decltype( _ldb.lower_bound( key ) )
    { try {
        return _ldb.lower_bound( key );
    } FC_CAPTURE_AND_RETHROW( (key) ) }
};

} } // bts::db
