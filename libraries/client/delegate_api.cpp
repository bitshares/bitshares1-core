#include <bts/client/client_impl.hpp>

using namespace bts::client;
using namespace bts::client::detail;

variant_object client_impl::delegate_get_settings()const
{ try {
    fc::mutable_variant_object settings;

    settings[ "network_min_connection_count" ]  = _min_delegate_connection_count;

    settings[ "block_max_production_time" ]     = _max_block_production_time;
    settings[ "block_max_transaction_count" ]   = _max_block_transaction_count;
    settings[ "block_max_size" ]                = _max_block_size;

    settings[ "transaction_max_size" ]          = _max_transaction_size;
    settings[ "transaction_min_fee" ]           = _min_transaction_fee;

    return settings;
} FC_CAPTURE_AND_RETHROW() }

uint32_t client_impl::delegate_set_network_min_connection_count( uint32_t count )
{ try {
    _min_delegate_connection_count = count;
    return _min_delegate_connection_count;
} FC_CAPTURE_AND_RETHROW( (count) ) }

uint32_t client_impl::delegate_set_block_max_production_time( uint32_t time )
{ try {
    const fc::microseconds max_block_production_time = fc::milliseconds( time );
    FC_ASSERT( max_block_production_time.to_seconds() <= BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC );
    _max_block_production_time = max_block_production_time;
    return _max_block_production_time.count() / 1000;
} FC_CAPTURE_AND_RETHROW( (time) ) }

uint32_t client_impl::delegate_set_block_max_transaction_count( uint32_t count )
{ try {
    _max_block_transaction_count = count;
    return _max_block_transaction_count;
} FC_CAPTURE_AND_RETHROW( (count) ) }

uint32_t client_impl::delegate_set_block_max_size( uint32_t size )
{ try {
    FC_ASSERT( size <= BTS_BLOCKCHAIN_MAX_BLOCK_SIZE );
    _max_block_size = size;
    return _max_block_size;
} FC_CAPTURE_AND_RETHROW( (size) ) }

uint32_t client_impl::delegate_set_transaction_max_size( uint32_t size )
{ try {
    _max_transaction_size = size;
    return _max_transaction_size;
} FC_CAPTURE_AND_RETHROW( (size) ) }

share_type client_impl::delegate_set_transaction_min_fee( const share_type& fee )
{ try {
    FC_ASSERT( fee >= BTS_BLOCKCHAIN_DEFAULT_RELAY_FEE );
    _min_transaction_fee = fee;
    return _min_transaction_fee;
} FC_CAPTURE_AND_RETHROW( (fee) ) }
