#pragma once


namespace bts { namespace dns { 

   class dns_cli : public bts::cli::cli
   {
      public:
         virtual void process_command( const std::string& cmd, const std::string& args );
         virtual void list_transactions( uint32_t count = 0 );
         virtual void get_balance( uint32_t min_conf, uint16_t unit = 0 );
   };

} } // bts::dns
