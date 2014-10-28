#include <bts/utilities/string_escape.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/stdio.hpp>
#include <fc/io/buffered_iostream.hpp>

#include <bts/client/messages.hpp>
#include <bts/net/message.hpp>

#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/tokenizer.hpp>

#include <iostream>
#include <fstream>
#include <list>
#include <set>



int main(int argc, char*argv[])
{
  for (;;)
  {
    fc::variant json_as_variant;
    try
    {
      fc::buffered_istream istream(fc::cin_ptr);
      json_as_variant = fc::json::from_stream(istream);
    }
    catch (const fc::eof_exception& e)
    {
      elog("EOF, exiting");
      return 0;
    }
    catch (const fc::exception& e)
    {
      elog("Error parsing json: ${what}", ("what", e.to_string())); 
      continue;
    }

    fc::variant_object json_as_variant_object;
    try
    {
      json_as_variant_object = json_as_variant.get_object();
    }
    catch (const fc::exception& e)
    {
      elog("Error interpreting json as object: ${what}", ("what", e.to_string()));
      continue;
    }

    if (json_as_variant_object.contains("trx"))
    {
      ilog("Object contains the key 'trx', assuming it is a trx_message");
      bts::client::trx_message trx_msg;
      try
      {
        trx_msg = json_as_variant.as<bts::client::trx_message>();
      }
      catch (const fc::exception& e)
      {
        elog("Error interpreting json as bts::client::trx_message: ${what}", ("what", e.to_string()));
      }

      ilog("Parsed bts::client::trx_message with id ${msg_id} containing transaction ${trx_id}",
        ("msg_id", bts::net::message(trx_msg).id())("trx_id", trx_msg.trx.id()));
    }
  }

#if 0

  // parse command-line options
  boost::program_options::options_description option_config("Allowed options");
  option_config.add_options()("help",                                                             "display this help message")
                             ("prologue"    ,  boost::program_options::value<std::string>(), "Add the contents of this file to start of generated C++ source")
                             ("epilogue"    ,  boost::program_options::value<std::string>(), "Add the contents of this file to end   of generated C++ source")
                             ("json"        ,  boost::program_options::value<std::string>(), "The json file to convert to C++ source code")
                             ("out"         ,  boost::program_options::value<std::string>(), "The file to generate");
  boost::program_options::variables_map option_variables;
  try
  {
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
      options(option_config).run(), option_variables);
    boost::program_options::notify(option_variables);
  }
  catch (boost::program_options::error&)
  {
    std::cerr << "Error parsing command-line options\n\n";
    std::cerr << option_config << "\n";
    return 1;
  }
#endif
  return 0;
}
