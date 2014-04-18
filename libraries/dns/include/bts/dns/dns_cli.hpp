#pragma once

#include <sstream>
#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/raw_variant.hpp>

#include <bts/cli/cli.hpp>
#include <bts/dns/dns_wallet.hpp>

namespace bts { namespace dns {

    using namespace client;

    class dns_cli : public bts::cli::cli
    {
        public:
            dns_cli (const client_ptr &c) : cli(c)
            {
            }

            virtual void process_command( const std::string& cmd, const std::string& args );
            virtual void list_transactions( uint32_t count = 0 );
            virtual void get_balance( uint32_t min_conf, uint16_t unit = 0 );
   };

} } // bts::dns
