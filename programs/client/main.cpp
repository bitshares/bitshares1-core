#include <boost/program_options.hpp>

#include <bts/client/client.hpp>
#include <bts/net/upnp.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/rpc/rpc_server.hpp>
#include <bts/cli/cli.hpp>
#include <bts/utilities/git_revision.hpp>
#include <fc/filesystem.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/git_revision.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger_config.hpp>

#include <boost/iostreams/tee.hpp>
#include <boost/iostreams/stream.hpp>
#include <fstream>

#include <iostream>
#include <iomanip>

int main( int argc, char** argv )
{
   try 
   {
      bts::client::client_ptr client = std::make_shared<bts::client::client>("bitshares_client");
      client->configure_from_command_line(argc,argv);
      client->start().wait();
   }
   catch ( const fc::exception& e )
   {
      std::cerr << "------------ error --------------\n" 
                << e.to_detail_string() << "\n";
      wlog( "${e}", ("e", e.to_detail_string() ) );
   }

   /* We need to shut down all the threads and tasks we can, things don't usually work very well when
      we rely on global destructors to handle cleanup */
   bts::blockchain::shutdown_ntp_time();

   // we should probably terminate the asio threads here

   ilog("Leaving main()");
   /*
    * We restore the initial logging config here in order to destroy all of the current
    * file_appender objects.  They are problematic because they have log rotation tasks
    * that they cancel in their destructors.  If we allow the file_appenders to continue
    * to exist and just shut down during global destructor, they will cancel their 
    * log rotation tasks and the canceled_exceptions will trigger log_contexts to be 
    * created.  These use fc::path which uses boost::filesystem::path, and that 
    * uses global objects in boost for character set conversion.  Since we're linking
    * boost statically, we can't control the order global destructors run in, and 
    * we have been crashing because the boost codecvt objects are already destroyed before
    * the file_appenders get destroyed.
    *
    * This could probably be resolved by linking boost dynamically.  This hack works,
    * but it prevents us from logging in global object destructors.  Probably we should
    * switch to dynamically linking in boost libraries.
    */
   fc::configure_logging(fc::logging_config::default_config());
   return 0;
}

