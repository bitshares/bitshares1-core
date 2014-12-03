
#include <bts/api/global_api_logger.hpp>

#if BTS_GLOBAL_API_LOG

#include <atomic>
#include <iostream>

#include <fc/exception/exception.hpp>

namespace bts { namespace api {

namespace detail
{
    class global_api_logger_impl : public global_api_logger
    {
    public:

        global_api_logger_impl();
        virtual ~global_api_logger_impl() override;

        virtual uint64_t log_call_started( const bts::api::common_api* target, const fc::string& name, const fc::variants& args ) override;
        virtual void log_call_finished( uint64_t call_id, const bts::api::common_api* target, const fc::string& name, const fc::variants& args, const fc::variant& result ) override;
        virtual bool obscure_passwords( ) const override;
        
        std::atomic_uint_fast64_t next_id;
    };

    global_api_logger_impl::global_api_logger_impl()
        : next_id(1)
    {
        return;
    }
    
    global_api_logger_impl::~global_api_logger_impl()
    {
        return;
    }
    
    uint64_t global_api_logger_impl::log_call_started( const bts::api::common_api* target, const fc::string& name, const fc::variants& args )
    {
        if( name == "debug_get_client_name" )
            return 0;
        uint64_t call_id = (uint64_t) next_id.fetch_add( 1 );
        for( api_logger* logger : this->active_loggers )
            logger->log_call_started( call_id, target, name, args );
        return call_id;
    }
    
    void global_api_logger_impl::log_call_finished( uint64_t call_id, const bts::api::common_api* target, const fc::string& name, const fc::variants& args, const fc::variant& result )
    {
        if( name == "debug_get_client_name" )
            return;
        for( api_logger* logger : this->active_loggers )
            logger->log_call_finished( call_id, target, name, args, result );
        return;
    }
    
    bool global_api_logger_impl::obscure_passwords( ) const
    {
        // TODO: Allow this to be overwritten
        return false;
    }

}

global_api_logger* global_api_logger::the_instance = new detail::global_api_logger_impl();

global_api_logger* global_api_logger::get_instance()
{
    return the_instance;
}

global_api_logger::global_api_logger()
{
    return;
}

global_api_logger::~global_api_logger()
{
    return;
}

api_logger::api_logger()
{
    this->is_connected = false;
    return;
}

api_logger::~api_logger()
{
    this->disconnect();
    return;
}

void api_logger::connect()
{
    if( this->is_connected )
        return;

    global_api_logger* g = global_api_logger::get_instance();
    if( g == NULL )
        return;
    
    g->active_loggers.push_back( this );
    this->is_connected = true;
    return;
}

void api_logger::disconnect()
{
    if( !this->is_connected )
        return;
    global_api_logger* g = global_api_logger::get_instance();
    if( g == NULL )
        return;

    size_t n = g->active_loggers.size();
    for( size_t i=0; i<n; i++ )
    {
        if( g->active_loggers[i] == this )
        {
            g->active_loggers.erase(g->active_loggers.begin() + i);
            break;
        }
    }

    n = g->active_loggers.size();
    for( size_t i=0; i<n; i++ )
    {
        FC_ASSERT( g->active_loggers[i] != this );
    }

    this->is_connected = false;

    return;
}

} }

#endif
