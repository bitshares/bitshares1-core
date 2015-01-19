#pragma once
#include <bts/db/level_map.hpp>

namespace bts { namespace db {

template<typename K, typename V>
class fast_level_map
{
    level_map<K, V>             _ldb;
    fc::optional<fc::path>      _ldb_path;
    bool                        _ldb_enabled = true;

    std::unordered_map<K, V>    _cache;

public:

    ~fast_level_map()
    {
        close();
    }

    void open( const fc::path& path )
    { try {
        FC_ASSERT( !_ldb_path.valid() );
        _ldb_path = path;
        _ldb.open( *_ldb_path );
        _cache.reserve( _ldb.size() );
        for( auto iter = _ldb.begin(); iter.valid(); ++iter )
            _cache.emplace( iter.key(), iter.value() );
    } FC_CAPTURE_AND_RETHROW( (path) ) }

    void close()
    { try {
        if( _ldb_path.valid() )
        {
            if( !_ldb_enabled ) toggle_leveldb( true );
            _ldb.close();
            _ldb_path = fc::optional<fc::path>();
        }
        _cache.clear();
    } FC_CAPTURE_AND_RETHROW() }

    void toggle_leveldb( const bool enabled )
    { try {
        FC_ASSERT( _ldb_path.valid() );
        if( enabled == _ldb_enabled )
            return;

        if( enabled )
        {
            _ldb.open( *_ldb_path );
            auto batch = _ldb.create_batch();
            for( const auto& item : _cache )
                batch.store( item.first, item.second );
            batch.commit();
        }
        else
        {
            _ldb.close();
            fc::remove_all( *_ldb_path );
        }

        _ldb_enabled = enabled;
    } FC_CAPTURE_AND_RETHROW( (enabled) ) }

    void store( const K& key, const V& value )
    { try {
        _cache[ key ] = value;
        if( _ldb_enabled )
            _ldb.store( key, value );
    } FC_CAPTURE_AND_RETHROW( (key)(value) ) }

    void remove( const K& key )
    { try {
        _cache.erase( key );
        if( _ldb_enabled )
            _ldb.remove( key );
    } FC_CAPTURE_AND_RETHROW( (key) ) }

    auto empty()const -> decltype( _cache.empty() )
    {
        return _cache.empty();
    }

    auto size()const -> decltype( _cache.size() )
    {
        return _cache.size();
    }

    auto count( const K& key )const -> decltype( _cache.count( key ) )
    {
        return _cache.count( key );
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
