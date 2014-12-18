#include <bts/client/client_impl.hpp>

namespace bts { namespace client { namespace detail {

#define UPDATE_CONFIG(name, value) delegate_config config = _delegate_config; \
                                   config.name = value; \
                                   config.validate(); \
                                   _delegate_config = config

fc::variant client_impl::delegate_get_config()const
{ try {
    return fc::variant( _delegate_config );
} FC_CAPTURE_AND_RETHROW() }

void client_impl::delegate_set_network_min_connection_count( uint32_t count )
{ try {
    UPDATE_CONFIG( network_min_connection_count, count );
} FC_CAPTURE_AND_RETHROW( (count) ) }

void client_impl::delegate_set_block_max_transaction_count( uint32_t count )
{ try {
    UPDATE_CONFIG( block_max_transaction_count, count );
} FC_CAPTURE_AND_RETHROW( (count) ) }

void client_impl::delegate_set_block_max_size( uint32_t size )
{ try {
    UPDATE_CONFIG( block_max_size, size );
} FC_CAPTURE_AND_RETHROW( (size) ) }

void client_impl::delegate_set_block_max_production_time( uint64_t time )
{ try {
    UPDATE_CONFIG( block_max_production_time, fc::microseconds( time ) );
} FC_CAPTURE_AND_RETHROW( (time) ) }

void client_impl::delegate_set_transaction_max_size( uint32_t size )
{ try {
    UPDATE_CONFIG( transaction_max_size, size );
} FC_CAPTURE_AND_RETHROW( (size) ) }

void client_impl::delegate_set_transaction_canonical_signatures_required( bool required )
{ try {
    UPDATE_CONFIG( transaction_canonical_signatures_required, required );
} FC_CAPTURE_AND_RETHROW( (required) ) }

void client_impl::delegate_set_transaction_min_fee( uint64_t fee )
{ try {
    UPDATE_CONFIG( transaction_min_fee, fee );
} FC_CAPTURE_AND_RETHROW( (fee) ) }

void client_impl::delegate_blacklist_add_transaction( const transaction_id_type& id )
{ try {
    _delegate_config.transaction_blacklist.insert( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

void client_impl::delegate_blacklist_remove_transaction( const transaction_id_type& id )
{ try {
    _delegate_config.transaction_blacklist.erase( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

void client_impl::delegate_blacklist_add_operation( const operation_type_enum& op )
{ try {
    _delegate_config.operation_blacklist.insert( op );
} FC_CAPTURE_AND_RETHROW( (op) ) }

void client_impl::delegate_blacklist_remove_operation( const operation_type_enum& op )
{ try {
    _delegate_config.operation_blacklist.erase( op );
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } } // bts::client::detail
