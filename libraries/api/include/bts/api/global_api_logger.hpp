
#pragma once

#if BTS_GLOBAL_API_LOG

#include <fc/variant.hpp>

namespace bts { namespace api {

class common_api;

// individual api_logger objects are called from singleton global_api_logger
class api_logger
{
public:

    api_logger();
    virtual ~api_logger();
    
    virtual void log_call_started ( uint64_t call_id, const bts::api::common_api* target, const fc::string& name, const fc::variants& args ) = 0;
    virtual void log_call_finished( uint64_t call_id, const bts::api::common_api* target, const fc::string& name, const fc::variants& args, const fc::variant& result ) = 0;

    virtual void connect();
    virtual void disconnect();

    bool is_connected;
};

class global_api_logger
{
public:

    global_api_logger();
    virtual ~global_api_logger();

    virtual uint64_t log_call_started( const bts::api::common_api* target, const fc::string& name, const fc::variants& args ) = 0;
    virtual void log_call_finished( uint64_t call_id, const bts::api::common_api* target, const fc::string& name, const fc::variants& args, const fc::variant& result ) = 0;
    virtual bool obscure_passwords( ) const = 0;

    static global_api_logger* get_instance();
    
    static global_api_logger* the_instance;

    std::vector< api_logger* > active_loggers;
};

} } // end namespace bts::api

#endif
