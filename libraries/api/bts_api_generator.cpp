#include <bts/api/api_metadata.hpp>
#include <bts/api/global_api_logger.hpp>
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


namespace bts { namespace api {


} } // end namespace bts::api

class type_mapping
{
private:
  std::string _type_name;
  std::string _to_variant_function;
  std::string _from_variant_function;
  bool _obscure_in_log_files;
public:
  type_mapping(const std::string& type_name) :
    _type_name(type_name),
    _obscure_in_log_files(false)
  {}
  std::string get_type_name() { return _type_name; }
  virtual std::string get_cpp_parameter_type() = 0;
  virtual std::string get_cpp_return_type() = 0;
  virtual void set_to_variant_function(const std::string& to_variant_function) { _to_variant_function = to_variant_function; }
  virtual void set_from_variant_function(const std::string& from_variant_function) { _from_variant_function = from_variant_function; }
  virtual void set_obscure_in_log_files() { _obscure_in_log_files = true; }
  virtual bool get_obscure_in_log_files() const { return _obscure_in_log_files; }
  virtual std::string convert_object_of_type_to_variant(const std::string& object_name);
  virtual std::string convert_variant_to_object_of_type(const std::string& variant_name);
  virtual std::string create_value_of_type_from_variant(const fc::variant& value);
};
typedef std::shared_ptr<type_mapping> type_mapping_ptr;

std::string type_mapping::convert_object_of_type_to_variant(const std::string& object_name)
{
  if (_to_variant_function.empty())
    return std::string("fc::variant(") + object_name + ")";
  else
    return _to_variant_function + "(" + object_name + ")";
}

std::string type_mapping::convert_variant_to_object_of_type(const std::string& variant_name)
{
  if (_from_variant_function.empty())
    return variant_name + ".as<" + get_cpp_return_type() + ">()";
  else
    return _from_variant_function + "(" + variant_name + ")";
}

std::string type_mapping::create_value_of_type_from_variant(const fc::variant& value)
{
  std::ostringstream default_value_as_variant;
  default_value_as_variant << "fc::json::from_string(" << bts::utilities::escape_string_for_c_source_code(fc::json::to_string(value)) << ")";
  return convert_variant_to_object_of_type(default_value_as_variant.str());
}


class void_type_mapping : public type_mapping
{
public:
  void_type_mapping() :
    type_mapping("void")
  {}
  virtual std::string get_cpp_parameter_type() override { FC_ASSERT(false, "can't use void as a parameter"); }
  virtual std::string get_cpp_return_type() override { return "void"; }
};

class fundamental_type_mapping : public type_mapping
{
private:
  std::string _cpp_parameter_type;
  std::string _cpp_return_type;
public:
  fundamental_type_mapping(const std::string& type_name, const std::string& parameter_type, const std::string& return_type) :
    type_mapping(type_name),
    _cpp_parameter_type(parameter_type),
    _cpp_return_type(return_type)
  {}
  virtual std::string get_cpp_parameter_type() override
  { 
    if (!_cpp_parameter_type.empty())
      return _cpp_parameter_type;
    return std::string("const ") + get_cpp_return_type() + "&";
  }
  virtual std::string get_cpp_return_type() override { return _cpp_return_type; }
};
typedef std::shared_ptr<fundamental_type_mapping> fundamental_type_mapping_ptr;

class sequence_type_mapping : public type_mapping
{
private:
  type_mapping_ptr _contained_type;
public:
  sequence_type_mapping(const std::string& name, const type_mapping_ptr& contained_type) :
    type_mapping(name),
    _contained_type(contained_type)
  {}
  virtual std::string get_cpp_parameter_type() override { return "const std::vector<" + _contained_type->get_cpp_return_type() + ">&"; }
  virtual std::string get_cpp_return_type() override { return "std::vector<" + _contained_type->get_cpp_return_type() + ">"; }
};
typedef std::shared_ptr<sequence_type_mapping> sequence_type_mapping_ptr;

class dictionary_type_mapping : public type_mapping
{
private:
  type_mapping_ptr _key_type;
  type_mapping_ptr _value_type;
public:
  dictionary_type_mapping(const std::string& name, const type_mapping_ptr& key_type, const type_mapping_ptr value_type) :
    type_mapping(name),
    _key_type(key_type),
    _value_type(value_type)
  {}
  virtual std::string get_cpp_parameter_type() override { return "const std::map<" + _key_type->get_cpp_return_type() + ", " + _value_type->get_cpp_return_type() + ">&"; }
  virtual std::string get_cpp_return_type() override { return "std::map<" + _key_type->get_cpp_return_type() + ", " + _value_type->get_cpp_return_type() + ">"; }
};
typedef std::shared_ptr<dictionary_type_mapping> dictionary_type_mapping_ptr;

struct parameter_description
{
  parameter_description():prompt(false){}

  std::string name;
  type_mapping_ptr type;
  std::string description;
  fc::optional<fc::variant> default_value;
  fc::optional<fc::variant> example;
  bool                      prompt;
};
typedef std::list<parameter_description> parameter_description_list;

struct method_description
{
  std::string name;
  std::string brief_description;
  std::string detailed_description;
  type_mapping_ptr return_type;
  parameter_description_list parameters;
  bool is_const;
  bts::api::method_prerequisites prerequisites; // actually, a bitmask of method_prerequisites
  std::vector<std::string> aliases;
  bool cached;
};
typedef std::list<method_description> method_description_list;

class api_generator
{
private:
  std::string _api_classname;

  typedef std::map<std::string, type_mapping_ptr> type_map_type;
  type_map_type _type_map;

  method_description_list _methods;
  std::set<std::string> _registered_method_names; // used for duplicate checking
  std::set<std::string> _include_files;
public:
  api_generator(const std::string& classname); 

  void load_api_description(const fc::path& api_description_filename, bool load_types);
  void generate_interface_file(const fc::path& api_description_output_dir, const std::string& generated_filename_suffix);
  void generate_rpc_client_files(const fc::path& rpc_client_output_dir, const std::string& generated_filename_suffix);
  void generate_rpc_server_files(const fc::path& rpc_server_output_dir, const std::string& generated_filename_suffix);
  void generate_client_files(const fc::path& client_output_dir, const std::string& generated_filename_suffix);
private:
  void write_includes_to_stream(std::ostream& stream);
  void generate_prerequisite_checks_to_stream(const method_description& method, std::ostream& stream);
  void generate_positional_server_implementation_to_stream(const method_description& method, const std::string& server_classname, std::ostream& stream);
  void generate_named_server_implementation_to_stream(const method_description& method, const std::string& server_classname, std::ostream& stream);
  void generate_server_call_to_client_to_stream(const method_description& method, std::ostream& stream);
  std::string generate_detailed_description_for_method(const method_description& method);
#if BTS_GLOBAL_API_LOG
  void create_global_api_entry_log_for_method( std::ostream& cpp_file, const fc::string& client_classname, const method_description& method );
  void create_global_api_exit_log_for_method( std::ostream& cpp_file, const fc::string& client_classname, const method_description& method );
#endif
  void write_generated_file_header(std::ostream& stream);
  std::string create_logging_statement_for_method(const method_description& method);

  type_mapping_ptr lookup_type_mapping(const std::string& type_string);
  void initialize_type_map_with_fundamental_types();
  void load_type_map(const fc::variants& json_type_map);
  parameter_description_list load_parameters(const fc::variants& json_parameter_descriptions);
  bts::api::method_prerequisites load_prerequisites(const fc::variant& json_prerequisites);
  void load_method_descriptions(const fc::variants& json_method_descriptions);
  std::string generate_signature_for_method(const method_description& method, const std::string& class_name, bool include_default_parameters);
};

std::string word_wrap(const std::string& text, const std::string& line_prefix, const std::string first_line_prefix = "", unsigned wrap_column = 120)
{
  std::ostringstream result;

  typedef boost::tokenizer<boost::char_separator<char> > char_tokenizer;
  boost::char_separator<char> whitespace_separator(" \t\n");
  char_tokenizer whitespace_tokenizer(text, whitespace_separator);

  unsigned current_column = 0;
  bool token_emitted_on_this_line = false;

  result << line_prefix;
  current_column += line_prefix.size();

  if (!first_line_prefix.empty())
  {
    result << first_line_prefix;
    current_column += first_line_prefix.size();
  }

  for (char_tokenizer::iterator tok_iter = whitespace_tokenizer.begin(); tok_iter != whitespace_tokenizer.end(); )
  {
    unsigned columns_left = wrap_column - current_column;
    unsigned this_token_size = tok_iter->size();
    if (token_emitted_on_this_line)
      ++this_token_size;
    if (this_token_size < columns_left)
    {
      if (token_emitted_on_this_line)
        result << " ";
      else
        token_emitted_on_this_line = true;
      result << *tok_iter;
      current_column += this_token_size;
      ++tok_iter;
      continue;
    } 
    else if (!token_emitted_on_this_line)
    {
      // this token is too long, but it will be too long for any line.
      // output it, then do our end of line processing
      result << *tok_iter;
      ++tok_iter;
    }
    result << "\n";
    current_column = 0;
    token_emitted_on_this_line = false;

    result << line_prefix;
    current_column += line_prefix.size();

    if (!first_line_prefix.empty())
      for (unsigned i = 0; i < first_line_prefix.size(); ++i)
        result << " ";
    current_column += first_line_prefix.size();
  }
  return result.str();
}


api_generator::api_generator(const std::string& classname) :
  _api_classname(classname)
{
  initialize_type_map_with_fundamental_types();
}

void api_generator::write_includes_to_stream(std::ostream& stream)
{
  for (const std::string& include_file : _include_files)
    stream << "#include <" << include_file << ">\n";
}

type_mapping_ptr api_generator::lookup_type_mapping(const std::string& type_string)
{
  auto iter = _type_map.find(type_string);
  FC_ASSERT(iter != _type_map.end(), "unable to find type mapping for type \"${type_name}\"", ("type_name", type_string));
  return iter->second;
}

void api_generator::load_type_map(const fc::variants& json_type_map)
{
  for (const fc::variant& json_type_variant : json_type_map)
  {
    fc::variant_object json_type = json_type_variant.get_object();
    FC_ASSERT(json_type.contains("type_name"), "type map entry is missing a \"type_name\" attribute");
    std::string json_type_name = json_type["type_name"].as_string();

    type_mapping_ptr mapping;

    if (json_type.contains("cpp_return_type"))
    {
      std::string parameter_type;
      if (json_type.contains("cpp_parameter_type"))
        parameter_type = json_type["cpp_parameter_type"].as_string();
      std::string return_type = json_type["cpp_return_type"].as_string();
      fundamental_type_mapping_ptr fundamental_mapping = std::make_shared<fundamental_type_mapping>(json_type_name, parameter_type, return_type);
      mapping = fundamental_mapping;
    }
    else if (json_type.contains("container_type"))
    {
      std::string container_type_string = json_type["container_type"].as_string();
      if (container_type_string == "array")
      {
        FC_ASSERT(json_type.contains("contained_type"), "arrays must specify a \"contained_type\"");
        std::string contained_type_name = json_type["contained_type"].as_string();
        type_mapping_ptr contained_type = lookup_type_mapping(contained_type_name);
        sequence_type_mapping_ptr sequence_mapping = std::make_shared<sequence_type_mapping>(json_type_name, contained_type);
        mapping = sequence_mapping;
      }
      else if (container_type_string == "dictionary")
      {
        FC_ASSERT(json_type.contains("key_type"), "dictionaries must specify a \"key_type\"");
        FC_ASSERT(json_type.contains("value_type"), "dictionaries must specify a \"value_type\"");
        std::string key_type_name = json_type["key_type"].as_string();
        std::string value_type_name = json_type["value_type"].as_string();
        type_mapping_ptr key_type = lookup_type_mapping(key_type_name);
        type_mapping_ptr value_type = lookup_type_mapping(value_type_name);
        dictionary_type_mapping_ptr dictionary_mapping = std::make_shared<dictionary_type_mapping>(json_type_name, key_type, value_type);
        mapping = dictionary_mapping;
      }
      else
        FC_THROW("invalid container_type for type ${type}", ("type", json_type_name));
    }
    else
      FC_THROW("malformed type map entry for type ${type}", ("type", json_type_name));

    if (json_type.contains("to_variant_function"))
      mapping->set_to_variant_function(json_type["to_variant_function"].as_string());
    if (json_type.contains("from_variant_function"))
      mapping->set_from_variant_function(json_type["from_variant_function"].as_string());
    if (json_type.contains("obscure_in_log_files") &&
        json_type["obscure_in_log_files"].as_bool())
      mapping->set_obscure_in_log_files();

    FC_ASSERT(_type_map.find(json_type_name) == _type_map.end(), 
              "Error, type ${type_name} is already registered", ("type_name", json_type_name));
    _type_map.insert(type_map_type::value_type(json_type_name, mapping));

    if (json_type.contains("cpp_include_file"))
      _include_files.insert(json_type["cpp_include_file"].as_string());
  }
}

parameter_description_list api_generator::load_parameters(const fc::variants& json_parameter_descriptions)
{
  parameter_description_list parameters;
  for (const fc::variant& parameter_description_variant : json_parameter_descriptions)
  {
    fc::variant_object json_parameter_description = parameter_description_variant.get_object();
    parameter_description parameter;
    FC_ASSERT(json_parameter_description.contains("name"), "parameter is missing \"name\"");
    parameter.name = json_parameter_description["name"].as_string();
    try
    {
      FC_ASSERT(json_parameter_description.contains("description"), "parameter is missing \"description\"");
      parameter.description = json_parameter_description["description"].as_string();
      FC_ASSERT(json_parameter_description.contains("type"), "parameter is missing \"type\"");
      parameter.type = lookup_type_mapping(json_parameter_description["type"].as_string());
      if( json_parameter_description.contains( "prompt" ) )
         parameter.prompt = json_parameter_description["prompt"].as_bool();
      if (json_parameter_description.contains("default_value"))
        parameter.default_value = json_parameter_description["default_value"];
      if (json_parameter_description.contains("example"))
        parameter.example = json_parameter_description["example"];
    }
    FC_RETHROW_EXCEPTIONS(error, "error processing parameter ${name}", ("name", parameter.name));
    parameters.push_back(parameter);
  }
  return parameters;
}

bts::api::method_prerequisites api_generator::load_prerequisites(const fc::variant& json_prerequisites)
{
  int result = 0;
  std::vector<bts::api::method_prerequisites> prereqs = json_prerequisites.as<std::vector<bts::api::method_prerequisites> >();
  for (bts::api::method_prerequisites prereq : prereqs)
    result |= prereq;
  return (bts::api::method_prerequisites)result;
}

void api_generator::load_method_descriptions(const fc::variants& method_descriptions)
{
  for (const fc::variant& method_description_variant : method_descriptions)
  {
    fc::variant_object json_method_description = method_description_variant.get_object();
    FC_ASSERT(json_method_description.contains("method_name"), "method entry missing \"method_name\"");
    std::string method_name = json_method_description["method_name"].as_string();
    FC_ASSERT(_registered_method_names.find(method_name) == _registered_method_names.end(),
              "Error: multiple methods registered with the name ${name}", ("name", method_name));
    _registered_method_names.insert(method_name);
    try
    {
      method_description method;
      method.name = method_name;
      FC_ASSERT(json_method_description.contains("return_type"), "method entry missing \"return_type\"");
      std::string return_type_name = json_method_description["return_type"].as_string();
      method.return_type = lookup_type_mapping(return_type_name);
      
      FC_ASSERT(json_method_description.contains("parameters"), "method entry missing \"parameters\"");
      method.parameters = load_parameters(json_method_description["parameters"].get_array());

      method.is_const = json_method_description.contains("is_const") && 
                               json_method_description["is_const"].as_bool();
      method.cached = json_method_description.contains("cached") && 
                               json_method_description["cached"].as_bool();

      FC_ASSERT(json_method_description.contains("prerequisites"), "method entry missing \"prerequisites\"");
      method.prerequisites = load_prerequisites(json_method_description["prerequisites"]);

      if (json_method_description.contains("aliases"))
      {
        method.aliases = json_method_description["aliases"].as<std::vector<std::string> >();
        for (const std::string& alias : method.aliases)
        {
          FC_ASSERT(_registered_method_names.find(alias) == _registered_method_names.end(),
                    "Error: alias \"${alias}\" conflicts with an existing method or alias", ("alias", alias));
          _registered_method_names.insert(alias);
        }
      }
      
      if (json_method_description.contains("description"))
        method.brief_description = json_method_description["description"].as_string();

      if (json_method_description.contains("detailed_description"))
        method.detailed_description = json_method_description["detailed_description"].as_string();

      _methods.push_back(method);
    }
    FC_RETHROW_EXCEPTIONS(warn, "error encountered parsing method description for method \"${method_name}\"", ("method_name", method_name));
  }
}

void api_generator::load_api_description(const fc::path& api_description_filename, bool load_types)
{
  try 
  {
    FC_ASSERT(fc::exists(api_description_filename), "Input file ${filename} does not exist", ("filename", api_description_filename));
    fc::variant_object api_description = fc::json::from_file(api_description_filename).get_object();
    if (load_types)
    {
      if (api_description.contains("type_map"))
        load_type_map(api_description["type_map"].get_array());
      return;
    }
    else
    {
      if (api_description.contains("methods"))
        load_method_descriptions(api_description["methods"].get_array());
    }
  } 
  FC_RETHROW_EXCEPTIONS(error, "Error parsing API description file ${filename}", ("filename", api_description_filename));
}

std::string api_generator::generate_signature_for_method(const method_description& method, const std::string& class_name, bool include_default_parameters)
{
  std::ostringstream method_signature;
  method_signature << method.return_type->get_cpp_return_type() << " ";
  if (!class_name.empty())
    method_signature << class_name << "::";
  method_signature << method.name << "(";
  bool first_parameter = true;
  for (const parameter_description& parameter : method.parameters)
  {
    if (first_parameter)
      first_parameter = false;
    else
      method_signature << ", ";
    method_signature << parameter.type->get_cpp_parameter_type() << " " << parameter.name;
    if (parameter.default_value)
    {
      if (!include_default_parameters)
        method_signature << " /*";
      method_signature << " = " << parameter.type->create_value_of_type_from_variant(*parameter.default_value);
      if (!include_default_parameters)
        method_signature << " */";
    }
  }
  method_signature << ")";
  if (method.is_const)
    method_signature << " const";
  return method_signature.str();
}

void api_generator::write_generated_file_header(std::ostream& stream)
{
  stream << "//                                   _           _    __ _ _      \n";
  stream << "//                                  | |         | |  / _(_) |     \n";
  stream << "//    __ _  ___ _ __   ___ _ __ __ _| |_ ___  __| | | |_ _| | ___ \n";
  stream << "//   / _` |/ _ \\ '_ \\ / _ \\ '__/ _` | __/ _ \\/ _` | |  _| | |/ _ \\`\n";
  stream << "//  | (_| |  __/ | | |  __/ | | (_| | ||  __/ (_| | | | | | |  __/\n";
  stream << "//   \\__, |\\___|_| |_|\\___|_|  \\__,_|\\__\\___|\\__,_| |_| |_|_|\\___|\n";
  stream << "//    __/ |                                                       \n";
  stream << "//   |___/                                                        \n";
  stream << "//\n";
  stream << "//\n";
  stream << "// Warning: this is a generated file, any changes made here will be\n";
  stream << "//          overwritten by the build process.  If you need to change what is\n";
  stream << "//          generated here, you should either modify the input json files\n";
  stream << "//          (network_api.json, wallet_api.json, etc) or modify the code\n";
  stream << "//          generator (bts_api_generator.cpp) itself\n";
  stream << "//\n";
}

void api_generator::generate_interface_file(const fc::path& api_description_output_dir, const std::string& generated_filename_suffix)
{
  fc::path api_description_header_path = api_description_output_dir / "include" / "bts" / "api";
  fc::create_directories(api_description_header_path);
  fc::path api_header_filename = api_description_header_path / (_api_classname + ".hpp");

  std::ofstream interface_file(api_header_filename.string() + generated_filename_suffix);
  write_generated_file_header(interface_file);

  interface_file << "#pragma once\n\n";
  write_includes_to_stream(interface_file);
  interface_file << "namespace bts { namespace api {\n\n";

  interface_file << "  class " << _api_classname << "\n";
  interface_file << "  {\n";
  interface_file << "  public:\n";

  for (const method_description& method : _methods)
  {
    interface_file << "    /**\n";
    if (!method.brief_description.empty())
    {
      std::ostringstream brief_description_doc_string;
      brief_description_doc_string << method.brief_description;
      if (method.brief_description.back() != '.')
        brief_description_doc_string << '.';
      interface_file << word_wrap(brief_description_doc_string.str(), "     * ") << "\n";
    }

    if (!method.detailed_description.empty())
    {
      std::istringstream detailed_description_stream(method.detailed_description);
      std::string line;
      while (std::getline(detailed_description_stream, line))
      {
        interface_file << "     *\n";
        interface_file << word_wrap(line, "     * ") << "\n";
      }
    }

    if (!method.parameters.empty())
      interface_file << "     *\n";
    for (const parameter_description& parameter : method.parameters)
    {
      std::ostringstream parameter_doc_string_rest;
      std::ostringstream parameter_doc_string_start;

      parameter_doc_string_start << "@param "  << parameter.name << " ";
      parameter_doc_string_rest << parameter.description << " (";
      if (parameter.default_value)
        parameter_doc_string_rest << parameter.type->get_type_name() << ", optional, defaults to " << fc::json::to_string(parameter.default_value);
      else
        parameter_doc_string_rest << parameter.type->get_type_name() << ", required";
      parameter_doc_string_rest << ")";
      interface_file << word_wrap(parameter_doc_string_rest.str(), "     * ", parameter_doc_string_start.str()) << "\n";      
    }
    if (!std::dynamic_pointer_cast<void_type_mapping>(method.return_type))
    {
      interface_file << "     *\n";
      std::ostringstream returns_doc_string;
      returns_doc_string << "@return " << method.return_type->get_type_name();
      interface_file << word_wrap(returns_doc_string.str(), "     * ") << "\n";      
    }

    interface_file << "     */\n";
    interface_file << "    virtual " << generate_signature_for_method(method, "", true) << " = 0;\n";
  }
  interface_file << "  };\n\n";
  interface_file << "} } // end namespace bts::api\n";
}

void api_generator::generate_rpc_client_files(const fc::path& rpc_client_output_dir, const std::string& generated_filename_suffix)
{
  std::string client_classname = _api_classname + "_rpc_client";

  fc::path client_header_path = rpc_client_output_dir / "include" / "bts" / "rpc_stubs";
  fc::create_directories(client_header_path); // creates dirs for both header and cpp
  fc::path client_header_filename = client_header_path / (client_classname + ".hpp");
  fc::path client_cpp_filename = rpc_client_output_dir / (client_classname + ".cpp");
  fc::path method_overrides_filename = client_header_path / (_api_classname + "_overrides.ipp");
  std::ofstream header_file(client_header_filename.string() + generated_filename_suffix);
  std::ofstream cpp_file(client_cpp_filename.string() + generated_filename_suffix);
  std::ofstream method_overrides_file(method_overrides_filename.string() + generated_filename_suffix);

  write_generated_file_header(header_file);
  write_generated_file_header(method_overrides_file);
  header_file << "#pragma once\n\n";
  header_file << "#include <fc/rpc/json_connection.hpp>\n";
  header_file << "#include <bts/api/" << _api_classname << ".hpp>\n\n";
  header_file << "namespace bts { namespace rpc_stubs {\n\n";
  header_file << "  class " << client_classname << " : public bts::api::" << _api_classname << "\n";
  header_file << "  {\n";
  header_file << "  public:\n";
  header_file << "    virtual fc::rpc::json_connection_ptr get_json_connection() const = 0;\n\n";
  for (const method_description& method : _methods)
  {
    header_file << "    " << generate_signature_for_method(method, "", true) << " override;\n";
    method_overrides_file << "    " << generate_signature_for_method(method, "", true) << " override;\n";
  }

  header_file << "  };\n\n";
  header_file << "} } // end namespace bts::rpc_stubs\n";

  write_generated_file_header(cpp_file);
  cpp_file << "#define DEFAULT_LOGGER \"rpc\"\n";
  cpp_file << "#include <bts/rpc_stubs/" << client_classname << ".hpp>\n";
  cpp_file << "#include <bts/api/conversion_functions.hpp>\n\n";
  cpp_file << "namespace bts { namespace rpc_stubs {\n\n";

  for (const method_description& method : _methods)
  {
    cpp_file << generate_signature_for_method(method, client_classname, false) << "\n";
    cpp_file << "{\n";

    cpp_file << "  fc::variant result = get_json_connection()->async_call(\"" << method.name << "\", std::vector<fc::variant>{";
    bool first = true;
    for (const parameter_description& parameter : method.parameters)
    {
       if( !first )
         cpp_file << ", ";
       first = false;
       cpp_file << parameter.type->convert_object_of_type_to_variant(parameter.name);
    }
    cpp_file << "}).wait();\n";

    if (!std::dynamic_pointer_cast<void_type_mapping>(method.return_type))
      cpp_file << "  return " << method.return_type->convert_variant_to_object_of_type("result") << ";\n";
    cpp_file << "}\n";
  }
  cpp_file << "\n";
  cpp_file << "} } // end namespace bts::rpc_stubs\n";
}

void api_generator::generate_prerequisite_checks_to_stream(const method_description& method, std::ostream& stream)
{
  if (method.prerequisites == bts::api::no_prerequisites)
    stream << "  // this method has no prerequisites\n\n";
  else
    stream << "  // check all of this method's prerequisites\n";

  if (method.prerequisites & bts::api::wallet_unlocked)
  {
    stream << "  verify_json_connection_is_authenticated(json_connection);\n";
    stream << "  verify_wallet_is_open();\n";
    stream << "  verify_wallet_is_unlocked();\n";
  }
  else if (method.prerequisites & bts::api::wallet_open)
  {
    stream << "  verify_json_connection_is_authenticated(json_connection);\n";
    stream << "  verify_wallet_is_open();\n";
  }
  else if (method.prerequisites & bts::api::json_authenticated)
  {
    stream << "  verify_json_connection_is_authenticated(json_connection);\n";
  }

  if (method.prerequisites & bts::api::connected_to_network)
    stream << "  verify_connected_to_network();\n";

  if (method.prerequisites != bts::api::no_prerequisites)
    stream << "  // done checking prerequisites\n\n";
}

void api_generator::generate_server_call_to_client_to_stream(const method_description& method, std::ostream& stream)
{
  stream << "\n";
  stream << "  ";
  if (!std::dynamic_pointer_cast<void_type_mapping>(method.return_type))
    stream << method.return_type->get_cpp_return_type() << " result = ";
  stream << "get_client()->" << method.name << "(";
  bool first_parameter = true;
  for (const parameter_description& parameter : method.parameters)
  {
    if (first_parameter)
      first_parameter = false;
    else
      stream << ", ";
    stream << parameter.name;
  }
  stream << ");\n";

  if (std::dynamic_pointer_cast<void_type_mapping>(method.return_type))
    stream << "  return fc::variant();\n";
  else
    stream << "  return fc::variant(result);\n";
}

void api_generator::generate_positional_server_implementation_to_stream(const method_description& method, const std::string& server_classname, std::ostream& stream)
{
  stream << "fc::variant " << server_classname << "::" << method.name << "_positional(fc::rpc::json_connection* json_connection, const fc::variants& parameters)\n";
  stream << "{\n";

  generate_prerequisite_checks_to_stream(method, stream);

  unsigned parameter_index = 0;
  for (const parameter_description& parameter : method.parameters)
  {
    std::ostringstream this_parameter;
    this_parameter << "parameters[" << parameter_index << "]";

    if (parameter.default_value)
    {
      stream << "  " << parameter.type->get_cpp_return_type() << " " << parameter.name << 
                  " = (parameters.size() <= " << parameter_index << ") ?\n";
      stream << "    (" << parameter.type->create_value_of_type_from_variant(*parameter.default_value) << ") :\n";
      stream << "    " << parameter.type->convert_variant_to_object_of_type(this_parameter.str()) << ";\n";
    }
    else
    {
      // parameter is required
      stream << "  if (parameters.size() <= " << parameter_index << ")\n";
      stream << "    FC_THROW_EXCEPTION(fc::invalid_arg_exception, \"missing required parameter " << (parameter_index + 1) << " (" << parameter.name << ")\");\n";
      stream << "  " << parameter.type->get_cpp_return_type() << " " << parameter.name << 
                  " = " << parameter.type->convert_variant_to_object_of_type(this_parameter.str()) << ";\n";
    }
    ++parameter_index;
  }

  generate_server_call_to_client_to_stream(method, stream);
  stream << "}\n\n";
}

void api_generator::generate_named_server_implementation_to_stream(const method_description& method, const std::string& server_classname, std::ostream& stream)
{
  stream << "fc::variant " << server_classname << "::" << method.name << "_named(fc::rpc::json_connection* json_connection, const fc::variant_object& parameters)\n";
  stream << "{\n";

  generate_prerequisite_checks_to_stream(method, stream);

  for (const parameter_description& parameter : method.parameters)
  {
    std::ostringstream this_parameter;
    this_parameter << "parameters[\"" << parameter.name << "\"]";

    if (parameter.default_value)
    {
      stream << "  " << parameter.type->get_cpp_return_type() << " " << parameter.name << 
                  " = parameters.contains(\"" << parameter.name << "\") ? \n";
      stream << "    (" << parameter.type->create_value_of_type_from_variant(*parameter.default_value) << ") :\n";
      stream << "    " << parameter.type->convert_variant_to_object_of_type(this_parameter.str()) << ";\n";
    }
    else
    {
      // parameter is required
      stream << "  if (!parameters.contains(\"" << parameter.name << "\"))\n";
      stream << "    FC_THROW_EXCEPTION(fc::invalid_arg_exception, \"missing required parameter '" <<  parameter.name << "'\");\n";
      stream << "  " << parameter.type->get_cpp_return_type() << " " << parameter.name << 
                  " = " << parameter.type->convert_variant_to_object_of_type(this_parameter.str()) << ";\n";
    }
  }

  generate_server_call_to_client_to_stream(method, stream);
  stream << "}\n\n";
}

std::string api_generator::generate_detailed_description_for_method(const method_description& method)
{
  std::ostringstream description;
  description << method.brief_description << "\n";

  if (!method.detailed_description.empty())
    description << "\n" << method.detailed_description << "\n";

  description << "\nParameters:\n";
  if (method.parameters.empty())
    description << "  (none)\n";
  else
  {
    for (const parameter_description& parameter : method.parameters)
    {
      description << "  " << parameter.name << " (" << parameter.type->get_type_name() << ", ";
      if (parameter.default_value)
        description << "optional, defaults to " << fc::json::to_string(parameter.default_value);
      else
        description << "required";
      description << "): " << parameter.description << "\n";
    }
  }

  description << "\nReturns:\n";
  description << "  " << method.return_type->get_type_name() << "\n";
  return description.str();
}

void api_generator::generate_rpc_server_files(const fc::path& rpc_server_output_dir, const std::string& generated_filename_suffix)
{
  std::string server_classname = _api_classname + "_rpc_server";

  fc::path server_header_path = rpc_server_output_dir / "include" / "bts" / "rpc_stubs";
  fc::create_directories(server_header_path); // creates dirs for both header and cpp
  fc::path server_header_filename = server_header_path / (server_classname + ".hpp");
  fc::path server_cpp_filename = rpc_server_output_dir / (server_classname + ".cpp");
  std::ofstream header_file(server_header_filename.string() + generated_filename_suffix);
  std::ofstream server_cpp_file(server_cpp_filename.string() + generated_filename_suffix);

  write_generated_file_header(header_file);
  header_file << "#pragma once\n";
  header_file << "#include <bts/api/api_metadata.hpp>\n";
  header_file << "#include <bts/api/common_api.hpp>\n";
  header_file << "#include <fc/rpc/json_connection.hpp>\n\n";
  header_file << "namespace bts { namespace rpc_stubs {\n";
  header_file << "  class " << server_classname << "\n";
  header_file << "  {\n";
  header_file << "  public:\n";
  header_file << "    virtual bts::api::common_api* get_client() const = 0;\n";
  header_file << "    virtual void verify_json_connection_is_authenticated(fc::rpc::json_connection* json_connection) const = 0;\n";
  header_file << "    virtual void verify_wallet_is_open() const = 0;\n";
  header_file << "    virtual void verify_wallet_is_unlocked() const = 0;\n";
  header_file << "    virtual void verify_connected_to_network() const = 0;\n\n";
  header_file << "    virtual void store_method_metadata(const bts::api::method_data& method_metadata) = 0;\n";
  header_file << "    fc::variant direct_invoke_positional_method(const std::string& method_name, const fc::variants& parameters);\n";
  header_file << "    void register_" << _api_classname << "_methods(const fc::rpc::json_connection_ptr& json_connection);\n\n";
  header_file << "    void register_" << _api_classname << "_method_metadata();\n\n";
  for (const method_description& method : _methods)
  {
    header_file << "    fc::variant " << method.name << "_positional(fc::rpc::json_connection* json_connection, const fc::variants& parameters);\n";
    header_file << "    fc::variant " << method.name << "_named(fc::rpc::json_connection* json_connection, const fc::variant_object& parameters);\n";
  }

  header_file << "  };\n\n";
  header_file << "} } // end namespace bts::rpc_stubs\n";

  write_generated_file_header(server_cpp_file);
  server_cpp_file << "#define DEFAULT_LOGGER \"rpc\"\n";
  server_cpp_file << "#include <bts/rpc_stubs/" << server_classname << ".hpp>\n";
  server_cpp_file << "#include <bts/api/api_metadata.hpp>\n";
  server_cpp_file << "#include <bts/api/conversion_functions.hpp>\n";
  server_cpp_file << "#include <boost/bind.hpp>\n";
  write_includes_to_stream(server_cpp_file);
  server_cpp_file << "\n";
  server_cpp_file << "namespace bts { namespace rpc_stubs {\n\n";

  // Generate the method bodies
  for (const method_description& method : _methods)
  {
    generate_positional_server_implementation_to_stream(method, server_classname, server_cpp_file);
    generate_named_server_implementation_to_stream(method, server_classname, server_cpp_file);
  }

  // Generate a function that registers all of the methods with the JSON-RPC dispatcher
  server_cpp_file << "void " << server_classname << "::register_" << _api_classname << "_methods(const fc::rpc::json_connection_ptr& json_connection)\n";
  server_cpp_file << "{\n";
  server_cpp_file << "  fc::rpc::json_connection::method bound_positional_method;\n";
  server_cpp_file << "  fc::rpc::json_connection::named_param_method bound_named_method;\n";
    server_cpp_file << "  auto capture_con = json_connection.get();\n ";
  for (const method_description& method : _methods)
  {
    server_cpp_file << "  // register method " << method.name << "\n";
    server_cpp_file << "  bound_positional_method = boost::bind(&" << server_classname << "::" << method.name << "_positional, \n";
    server_cpp_file << "                                        this, capture_con, _1);\n";
    server_cpp_file << "  json_connection->add_method(\"" << method.name << "\", bound_positional_method);\n";
    for (const std::string& alias : method.aliases)
      server_cpp_file << "  json_connection->add_method(\"" << alias << "\", bound_positional_method);\n";
    server_cpp_file << "  bound_named_method = boost::bind(&" << server_classname << "::" << method.name << "_named, \n";
    server_cpp_file << "                                        this, capture_con, _1);\n";
    server_cpp_file << "  json_connection->add_named_param_method(\"" << method.name << "\", bound_named_method);\n";
    for (const std::string& alias : method.aliases)
      server_cpp_file << "  json_connection->add_named_param_method(\"" << alias << "\", bound_named_method);\n";
    server_cpp_file << "\n";
  }
  server_cpp_file << "}\n\n";

  // generate a function that registers all method metadata
  server_cpp_file << "void " << server_classname << "::register_" << _api_classname << "_method_metadata()\n";
  server_cpp_file << "{\n";
  for (const method_description& method : _methods)
  {
    server_cpp_file << "  {\n";
    server_cpp_file << "    // register method " << method.name << "\n";
    server_cpp_file << "    bts::api::method_data " << method.name << "_method_metadata{\"" << method.name << "\", nullptr,\n";
    server_cpp_file << "      /* description */ " << bts::utilities::escape_string_for_c_source_code(method.brief_description) << ",\n";
    server_cpp_file << "      /* returns */ \"" << method.return_type->get_type_name() << "\",\n";
    server_cpp_file << "      /* params: */ {";
    bool first_parameter = true;
    for (const parameter_description& parameter : method.parameters)
    {
      if (first_parameter)
        first_parameter = false;
      else
        server_cpp_file << ",";
      server_cpp_file << "\n        {\"" << parameter.name << "\", \"" << parameter.type->get_type_name() <<  "\", bts::api::";
      if (parameter.default_value)
        server_cpp_file << "optional_positional, fc::variant(fc::json::from_string(" << bts::utilities::escape_string_for_c_source_code(fc::json::to_string(parameter.default_value)) << "))}";
      else
        server_cpp_file << "required_positional, fc::ovariant()}";
    }
    if( !first_parameter )
      server_cpp_file << "\n      ";
    server_cpp_file << "},\n";
    server_cpp_file << "      /* prerequisites */ (bts::api::method_prerequisites) " << (int)method.prerequisites << ",\n";
    server_cpp_file << "      /* detailed description */ " << bts::utilities::escape_string_for_c_source_code(generate_detailed_description_for_method(method)) << ",\n";
    server_cpp_file << "      /* aliases */ {";
    if (!method.aliases.empty())
    {
      bool first = true;
      for (const std::string& alias : method.aliases)
      {
        if (first)
          first = false;
        else
          server_cpp_file << ", ";
        server_cpp_file << "\"" << alias << "\"";
      }
    }
    server_cpp_file << "}, " << (method.cached ? "true" : "false")<<"};\n";
      
    server_cpp_file << "    store_method_metadata(" << method.name << "_method_metadata);\n";
    server_cpp_file << "  }\n\n";
  }
  server_cpp_file << "}\n\n";

  // generate a function for directly invoking a method, probably a stop-gap until we finish migrating all methods to this code
  server_cpp_file << "fc::variant " << server_classname << "::direct_invoke_positional_method(const std::string& method_name, const fc::variants& parameters)\n";
  server_cpp_file << "{\n";
  for (const method_description& method : _methods)
  {
    server_cpp_file << "  if (method_name == \"" << method.name << "\")\n";
    server_cpp_file << "    return " << method.name << "_positional(nullptr, parameters);\n";
  }
  server_cpp_file << "  FC_ASSERT(false, \"shouldn't happen\");\n";
  server_cpp_file << "}\n";

  server_cpp_file << "\n";
  server_cpp_file << "} } // end namespace bts::rpc_stubs\n";
}

#if BTS_GLOBAL_API_LOG
void api_generator::create_global_api_entry_log_for_method(
    std::ostream& cpp_file,
    const fc::string& client_classname,
    const method_description& method
    )
{
  cpp_file << "  bts::api::global_api_logger* glog = bts::api::global_api_logger::get_instance();\n"
              "  uint64_t call_id = 0;\n"
              "  fc::variants args;\n"
              "  if( glog != NULL )\n"
              "  {\n";
  
  for( const parameter_description& param : method.parameters )
  {
    if( param.type->get_obscure_in_log_files() )
    {
      cpp_file << "    if( glog->obscure_passwords() )\n"
                  "      args.push_back( fc::variant(\"*********\") );\n"
                  "    else\n"
                  "      ";
    }
    else
      cpp_file << "    ";

    cpp_file << "args.push_back( " <<
        param.type->convert_object_of_type_to_variant( param.name )
        << " );\n";
  }
  cpp_file << "    call_id = glog->log_call_started( this, \"" << method.name << "\", args );\n"
              "  }\n";
  return;
}

void api_generator::create_global_api_exit_log_for_method(
    std::ostream& cpp_file,
    const fc::string& client_classname,
    const method_description& method
    )
{
  cpp_file << "    if( call_id != 0 )\n"
              "      glog->log_call_finished( call_id, this, \"" << method.name << "\", args, "
           << method.return_type->convert_object_of_type_to_variant( "result" )
           << " );\n";
  return;
}
#endif

void api_generator::generate_client_files(const fc::path& client_output_dir, const std::string& generated_filename_suffix)
{
  fc::path client_header_path = client_output_dir / "include" / "bts" / "rpc_stubs";
  fc::create_directories(client_header_path); // creates dirs for both header and cpp

  std::string interceptor_classname = _api_classname + "_client";
  fc::path interceptor_header_filename = client_header_path / (interceptor_classname + ".hpp");
  std::ofstream interceptor_header_file(interceptor_header_filename.string() + generated_filename_suffix);
  fc::path interceptor_cpp_filename = client_output_dir / (interceptor_classname + ".cpp");
  std::ofstream interceptor_cpp_file(interceptor_cpp_filename.string() + generated_filename_suffix);
  
  write_generated_file_header(interceptor_header_file);
  interceptor_header_file << "#pragma once\n";
  interceptor_header_file << "#include <bts/api/api_metadata.hpp>\n";
  interceptor_header_file << "#include <bts/api/common_api.hpp>\n\n";

  interceptor_header_file << "namespace bts { namespace rpc_stubs {\n";
  interceptor_header_file << "  class " << interceptor_classname << " : public bts::api::common_api\n";
  interceptor_header_file << "  {\n";
  interceptor_header_file << "  protected:\n";
  interceptor_header_file << "    virtual bts::api::common_api* get_impl() const = 0;\n\n";
  interceptor_header_file << "  public:\n";

  for (const method_description& method : _methods)
    interceptor_header_file << "    " << generate_signature_for_method(method, "", true) << " override;\n";

  interceptor_header_file << "  };\n\n";
  interceptor_header_file << "} } // end namespace bts::rpc_stubs\n";

  interceptor_cpp_file << "#define DEFAULT_LOGGER \"rpc\"\n";
#if BTS_GLOBAL_API_LOG
  interceptor_cpp_file << "#include <bts/api/global_api_logger.hpp>\n";
  interceptor_cpp_file << "#include <bts/api/conversion_functions.hpp>\n";
#endif
  interceptor_cpp_file << "#include <bts/rpc_stubs/" << interceptor_classname << ".hpp>\n\n";
  interceptor_cpp_file << "namespace bts { namespace rpc_stubs {\n\n";

  for (const method_description& method : _methods)
  {
    interceptor_cpp_file << generate_signature_for_method(method, interceptor_classname, false) << "\n";
    interceptor_cpp_file << "{\n";
    interceptor_cpp_file << "  " << create_logging_statement_for_method(method) << "\n";
#if BTS_GLOBAL_API_LOG
    create_global_api_entry_log_for_method( interceptor_cpp_file, interceptor_classname, method );
    interceptor_cpp_file << "\n";
#endif
    interceptor_cpp_file << "  struct scope_exit\n";
    interceptor_cpp_file << "  {\n";
    interceptor_cpp_file << "    fc::time_point start_time;\n";
    interceptor_cpp_file << "    scope_exit() : start_time(fc::time_point::now()) {}\n";
    interceptor_cpp_file << "    ~scope_exit() { dlog(\"RPC call " << method.name << " finished in ${time} ms\", (\"time\", (fc::time_point::now() - start_time).count() / 1000)); }\n";
    interceptor_cpp_file << "  } execution_time_logger;\n";
    interceptor_cpp_file << "  try\n";
    interceptor_cpp_file << "  {\n";
    interceptor_cpp_file << "    ";
    bool is_void = !!std::dynamic_pointer_cast<void_type_mapping>(method.return_type);
#if BTS_GLOBAL_API_LOG
    if( !is_void )
      interceptor_cpp_file << method.return_type->get_cpp_return_type() << " result = ";
    else
      interceptor_cpp_file << "std::nullptr_t result = nullptr;\n    ";
#else
    if( !is_void )
      interceptor_cpp_file << "return ";
#endif
    std::list<std::string> args;
    for (const parameter_description& param : method.parameters)
      args.push_back(param.name);
    interceptor_cpp_file << "get_impl()->" << method.name << "(" << boost::join(args, ", ") << ");\n";
#if BTS_GLOBAL_API_LOG
    create_global_api_exit_log_for_method( interceptor_cpp_file, interceptor_classname, method );
    if( !is_void )
      interceptor_cpp_file << "\n    return result;\n";
    else
      interceptor_cpp_file << "\n    return;\n";
#endif
    interceptor_cpp_file << "  }\n";
    interceptor_cpp_file << "  FC_RETHROW_EXCEPTIONS(warn, \"\")\n";
    interceptor_cpp_file << "}\n\n";
  }
  interceptor_cpp_file << "} } // end namespace bts::rpc_stubs\n";
}

std::string api_generator::create_logging_statement_for_method(const method_description& method)
{
  std::ostringstream result;

  std::list<std::string> args;
  for (const parameter_description& param :method.parameters)
  {
    if (param.type->get_obscure_in_log_files())
      args.push_back("*********");
    else
      args.push_back(std::string("${") + param.name + "}");
  }
  result << "ilog(\"received RPC call: " << method.name << "(" << boost::join(args, ", ") << ")\", ";
  for (const parameter_description& param :method.parameters)
    if (!param.type->get_obscure_in_log_files())
      result << "(\"" << param.name << "\", " << param.name << ")";
  result << ");";

  return result.str();
}

void api_generator::initialize_type_map_with_fundamental_types()
{
  const char* pass_by_value_types[] =
  {
    "int8_t",
    "uint8_t",
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t",
    "bool"
  };
  const char* pass_by_reference_types[] =
  {
    "std::string",
  };

  _type_map.insert(type_map_type::value_type("void", std::make_shared<void_type_mapping>()));
  for (const char* by_value_type : pass_by_value_types)
    _type_map.insert(type_map_type::value_type(by_value_type, std::make_shared<fundamental_type_mapping>(by_value_type, by_value_type, by_value_type)));
  for (const char* by_value_type : pass_by_reference_types)
    _type_map.insert(type_map_type::value_type(by_value_type, std::make_shared<fundamental_type_mapping>(by_value_type, std::string("const ") + by_value_type + "&", by_value_type)));
  _type_map.insert(type_map_type::value_type("json_variant", std::make_shared<fundamental_type_mapping>("json_variant", "const fc::variant&", "fc::variant")));
  _type_map.insert(type_map_type::value_type("json_variant_array", std::make_shared<sequence_type_mapping>("json_variant_array", _type_map["json_variant"])));
  _type_map.insert(type_map_type::value_type("json_object", std::make_shared<fundamental_type_mapping>("json_object", "const fc::variant_object&", "fc::variant_object")));
  _type_map.insert(type_map_type::value_type("json_object_array", std::make_shared<sequence_type_mapping>("json_object_array", _type_map["json_object"])));
}

int main(int argc, char*argv[])
{
  // parse command-line options
  boost::program_options::options_description option_config("Allowed options");
  option_config.add_options()("help",                                                             "display this help message")
                             ("api-description",    boost::program_options::value<std::vector<std::string> >(), "API description file name in JSON format")
                             ("api-interface-output-dir", boost::program_options::value<std::string>(), "C++ pure interface class output directory")
                             ("api-classname",      boost::program_options::value<std::string>(), "Name for the generated C++ classes")
                             ("rpc-stub-output-dir", boost::program_options::value<std::string>(), "Output directory for RPC server/client stubs")
                             ("generated-file-suffix", boost::program_options::value<std::string>()->default_value(""), "Suffix to append to generated filenames");
  boost::program_options::positional_options_description positional_config;
  positional_config.add("api-description", -1);

  boost::program_options::variables_map option_variables;
  try
  {
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
      options(option_config).positional(positional_config).run(), option_variables);
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

  if (!option_variables.count("api-classname"))
  {
    std::cout << "Missing argument --api-classname\n";
    return 1;
  }
  
  api_generator generator(option_variables["api-classname"].as<std::string>());

  if (!option_variables.count("api-description"))
  {
    std::cout << "Missing argument --api-description\n";
    return 1;    
  }

  try
  {
    std::vector<std::string> api_description_filenames = option_variables["api-description"].as<std::vector<std::string> >();
    // make two passes, first loading all types, second loading all methods.  This allows methods in one file
    // to use types that might be declared in a file loaded later
    for (const std::string& api_description_filename : api_description_filenames)
      generator.load_api_description(api_description_filename, true);
    for (const std::string& api_description_filename : api_description_filenames)
      generator.load_api_description(api_description_filename, false);

    if (option_variables.count("api-interface-output-dir"))
      generator.generate_interface_file(option_variables["api-interface-output-dir"].as<std::string>(), option_variables["generated-file-suffix"].as<std::string>());

    if (option_variables.count("rpc-stub-output-dir"))
    {
      generator.generate_rpc_client_files(option_variables["rpc-stub-output-dir"].as<std::string>(), option_variables["generated-file-suffix"].as<std::string>());
      generator.generate_rpc_server_files(option_variables["rpc-stub-output-dir"].as<std::string>(), option_variables["generated-file-suffix"].as<std::string>());
      generator.generate_client_files(option_variables["rpc-stub-output-dir"].as<std::string>(), option_variables["generated-file-suffix"].as<std::string>());
    }
  }
  catch (const fc::exception& e)
  {
    elog("Caught while generating API: ${msg}", ("msg", e.to_detail_string()));
    return 1;
  }
  return 0;
}
