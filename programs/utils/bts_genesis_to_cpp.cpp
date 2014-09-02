#include <bts/utilities/string_escape.hpp>
#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/tokenizer.hpp>

#include <iostream>
#include <fstream>
#include <list>
#include <set>



void compile_json_to_source(
    const fc::path& source_file_name,
    const fc::path& prologue_file_name,
    const fc::path& json_file_name,
    const fc::path& epilogue_file_name
    )
{
  fc::mutable_variant_object template_context;

  template_context[  "source_file_name"] =   source_file_name.string();
  template_context["prologue_file_name"] = prologue_file_name.string();
  template_context[    "json_file_name"] =     json_file_name.string();
  template_context["epilogue_file_name"] = epilogue_file_name.string();

  std::ofstream source_file(source_file_name.string());

  if (prologue_file_name.string() != "")
  {
    std::ifstream prologue_file(prologue_file_name.string());
    std::stringstream ss_prologue_file;
    ss_prologue_file << prologue_file.rdbuf();
    source_file << format_string(ss_prologue_file.str(), template_context);
  }

  std::ifstream json_file(json_file_name.string());

  std::string line;
  bool first = true;
  while (std::getline(json_file, line))
  {
    if (first)
      first = false;
    else
      source_file << ",\n";
    source_file << "  " << bts::utilities::escape_string_for_c_source_code(line);
  }

  if (epilogue_file_name.string() != "")
  {
    std::ifstream epilogue_file(epilogue_file_name.string());
    std::stringstream ss_epilogue_file;
    ss_epilogue_file << epilogue_file.rdbuf();
    source_file << format_string(ss_epilogue_file.str(), template_context);
  }

  return;
}

int main(int argc, char*argv[])
{
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

  fc::path source_file_name;
  fc::path prologue_file_name;
  fc::path json_file_name;
  fc::path epilogue_file_name;

  if (option_variables.count("help"))
  {
    std::cout << option_config << "\n";
    return 0;
  }

  if (!option_variables.count("prologue"))
    prologue_file_name = "";
  else
    prologue_file_name = option_variables["prologue"].as<std::string>();

  if (!option_variables.count("json"))
  {
    std::cout << "Missing argument --json\n";
    return 1;
  }
  json_file_name = option_variables["json"].as<std::string>();

  if (!option_variables.count("out"))
  {
    std::cout << "Missing argument --out\n";
    return 1;
  }

  source_file_name = option_variables["out"].as<std::string>();

  if (!option_variables.count("epilogue"))
    epilogue_file_name = "";
  else
    epilogue_file_name = option_variables["epilogue"].as<std::string>();

  try
  {
    compile_json_to_source(source_file_name, prologue_file_name, json_file_name, epilogue_file_name);
  }
  catch (const fc::exception& e)
  {
    elog("Caught while compiling genesis block: ${msg}", ("msg", e.to_detail_string()));
    return 1;
  }
  return 0;
}
