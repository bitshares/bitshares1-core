#include <bts/blockchain/address.hpp>
#include <bts/blockchain/genesis_config.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>

#include <fc/io/raw.hpp>
#include <iostream>
#include <iomanip>
#include <fstream>

#include <boost/program_options.hpp>

using namespace bts::blockchain;

int main( int argc, char** argv )
{
  boost::program_options::options_description option_config("Allowed options");
  option_config.add_options()("help",                                                        "display this help message")
                             ("prologue"    ,  boost::program_options::value<std::string>(), "Add the contents of this file to start of generated C++ source")
                             ("epilogue"    ,  boost::program_options::value<std::string>(), "Add the contents of this file to end   of generated C++ source")
                             ("json"        ,  boost::program_options::value<std::string>(), "The json file to convert to C++ source code")
                             ("binary-out"  ,  boost::program_options::value<std::string>(), "The raw binary file to generate")
                             ("source-out"  ,  boost::program_options::value<std::string>(), "The C++ source file to generate");
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

  if (option_variables.count("help"))
  {
    std::cout << option_config << "\n";
    return 0;
  }

  if (!option_variables.count("json"))
  {
    std::cout << "error: missing --json argument\n";
    return 2;
  }

  genesis_block_config genesis_config = fc::json::from_file(option_variables["json"].as<std::string>()).as<genesis_block_config>();

  if (option_variables.count("binary-out"))
  {
    std::ofstream genesis_bin(option_variables["binary-out"].as<std::string>());
    fc::raw::pack(genesis_bin, genesis_config);
  }

  if (option_variables.count("source-out"))
  {
    std::ofstream source_out(option_variables["source-out"].as<std::string>());

    if (option_variables.count("prologue"))
    {
      std::ifstream prologue(option_variables["prologue"].as<std::string>());
      std::copy(std::istreambuf_iterator<char>(prologue.rdbuf()),
                std::istreambuf_iterator<char>(),
                std::ostream_iterator<char>(source_out));
    }

    std::stringstream genesis_bin;
    fc::raw::pack(genesis_bin, genesis_config);
    int first = true;
    int column = 0;
    std::istreambuf_iterator<char> end_iter;
    for (std::istreambuf_iterator<char> iter(genesis_bin.rdbuf());
         iter != end_iter; ++iter)    
    {
      if (first)
      {
        first = false;
        source_out << "  ";
        column += 2;
      }
      else
      {
        source_out << ", ";
        column += 2;

        if (column > 75)
        {
          source_out << "\n  ";
          column = 2;
        }
      }
      source_out << "0x" << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)*iter;
      column += 4;
    }
    source_out << "\n";

    if (option_variables.count("epilogue"))
    {
      std::ifstream epilogue(option_variables["epilogue"].as<std::string>());
      std::copy(std::istreambuf_iterator<char>(epilogue.rdbuf()),
                std::istreambuf_iterator<char>(),
                std::ostream_iterator<char>(source_out));
    }
  }

  return 0;
}
