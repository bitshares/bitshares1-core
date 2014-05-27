#include <fc/optional.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>
#include <fc/exception/exception.hpp>

#include <boost/program_options.hpp>

#include <fstream>

namespace bts { namespace api {


} } // end namespace bts::api

class type_mapping
{
private:
  std::string _type_name;
public:
  type_mapping(const std::string& type_name) :
    _type_name(type_name)
  {}
  std::string get_type_name() { return _type_name; }
  virtual std::string get_cpp_parameter_type() = 0;
  virtual std::string get_cpp_return_type() = 0;
};
typedef std::shared_ptr<type_mapping> type_mapping_ptr;

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
  virtual std::string get_cpp_parameter_type() override { return _cpp_parameter_type; }
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
  std::string name;
  type_mapping_ptr type;
  std::string description;
  fc::optional<std::string> default_value;
  fc::optional<std::string> example;
};
typedef std::list<parameter_description> parameter_description_list;

struct method_description
{
  std::string name;
  type_mapping_ptr return_type;
  parameter_description_list parameters;
};
typedef std::list<method_description> method_description_list;

class api_generator
{
private:
  std::string _api_classname;

  typedef std::map<std::string, type_mapping_ptr> type_map_type;
  type_map_type _type_map;

  method_description_list _methods;
public:
  api_generator(const std::string& classname); 

  void load_api_description(const fc::path& api_description_filename);
  void generate_interface_file(const fc::path& api_description_filename);
  void generate_client_files(const fc::path& client_header_filename, const fc::path& client_cpp_filename);
  void generate_server_files(const fc::path& server_header_filename, const fc::path& server_cpp_filename);
private:
  type_mapping_ptr lookup_type_mapping(const std::string& type_string);
  void initialize_type_map_with_fundamental_types();
  void load_type_map(const fc::variants& json_type_map);
  parameter_description_list load_parameters(const fc::variants& json_parameter_descriptions);
  void load_method_descriptions(const fc::variants& json_method_descriptions);
  std::string generate_signature_for_method(const method_description& method, const std::string& class_name, bool include_default_parameters);
};

api_generator::api_generator(const std::string& classname) :
  _api_classname(classname)
{
  initialize_type_map_with_fundamental_types();
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
    FC_ASSERT(json_type.contains("type_name"));
    std::string json_type_name = json_type["type_name"].as_string();

    if (json_type.contains("cpp_parameter_type") && 
        json_type.contains("cpp_return_type"))
    {
      std::string parameter_type = json_type["cpp_parameter_type"].as_string();
      std::string return_type = json_type["cpp_return_type"].as_string();
      fundamental_type_mapping_ptr mapping = std::make_shared<fundamental_type_mapping>(json_type_name, parameter_type, return_type);
      _type_map.insert(type_map_type::value_type(json_type_name, mapping));
    }
    else if (json_type.contains("container_type"))
    {
      std::string container_type_string = json_type["container_type"].as_string();
      if (container_type_string == "sequence")
      {
        FC_ASSERT(json_type.contains("contained_type"));
        std::string contained_type_name = json_type["contained_type"].as_string();
        type_mapping_ptr contained_type = lookup_type_mapping(contained_type_name);
        sequence_type_mapping_ptr mapping = std::make_shared<sequence_type_mapping>(json_type_name, contained_type);
        _type_map.insert(type_map_type::value_type(json_type_name, mapping));
      }
      else if (container_type_string == "dictionary")
      {
        FC_ASSERT(json_type.contains("key_type"));
        FC_ASSERT(json_type.contains("value_type"));
        std::string key_type_name = json_type["key_type"].as_string();
        std::string value_type_name = json_type["value_type"].as_string();
        type_mapping_ptr key_type = lookup_type_mapping(key_type_name);
        type_mapping_ptr value_type = lookup_type_mapping(value_type_name);
        dictionary_type_mapping_ptr mapping = std::make_shared<dictionary_type_mapping>(json_type_name, key_type, value_type);
        _type_map.insert(type_map_type::value_type(json_type_name, mapping));
      }
      else
        FC_THROW("invalid container_type for type ${type}", ("type", json_type_name));
    }
    else
      FC_THROW("malformed type map entry for type ${type}", ("type", json_type_name));
  }
}

parameter_description_list api_generator::load_parameters(const fc::variants& json_parameter_descriptions)
{
  parameter_description_list parameters;
  for (const fc::variant& parameter_description_variant : json_parameter_descriptions)
  {
    fc::variant_object json_parameter_description = parameter_description_variant.get_object();
    parameter_description parameter;
    FC_ASSERT(json_parameter_description.contains("name"));
    parameter.name = json_parameter_description["name"].as_string();
    FC_ASSERT(json_parameter_description.contains("description"));
    parameter.description = json_parameter_description["description"].as_string();
    FC_ASSERT(json_parameter_description.contains("type"));
    parameter.type = lookup_type_mapping(json_parameter_description["type"].as_string());
    if (json_parameter_description.contains("default_value"))
      parameter.default_value = json_parameter_description["default_value"].as_string();
    if (json_parameter_description.contains("example"))
      parameter.example = json_parameter_description["example"].as_string();

    parameters.push_back(parameter);
  }
  return parameters;
}

void api_generator::load_method_descriptions(const fc::variants& method_descriptions)
{
  for (const fc::variant& method_description_variant : method_descriptions)
  {
    fc::variant_object json_method_description = method_description_variant.get_object();
    FC_ASSERT(json_method_description.contains("method_name"));
    std::string method_name = json_method_description["method_name"].as_string();
    try
    {
      method_description method;
      method.name = method_name;
      FC_ASSERT(json_method_description.contains("return_type"));
      std::string return_type_name = json_method_description["return_type"].as_string();
      method.return_type = lookup_type_mapping(return_type_name);
      
      FC_ASSERT(json_method_description.contains("parameters"));
      method.parameters = load_parameters(json_method_description["parameters"].get_array());

      _methods.push_back(method);
    }
    FC_RETHROW_EXCEPTIONS(warn, "error encountered parsing method description for method \"${method_name}\"", ("method_name", method_name));
  }
}

void api_generator::load_api_description(const fc::path& api_description_filename)
{
  FC_ASSERT(fc::exists(api_description_filename));
  fc::variant_object api_description = fc::json::from_file(api_description_filename).get_object();
  if (api_description.contains("type_map"))
    load_type_map(api_description["type_map"].get_array());
  FC_ASSERT(api_description.contains("methods"));
  load_method_descriptions(api_description["methods"].get_array());
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
      method_signature << " = " << *parameter.default_value;
      if (!include_default_parameters)
        method_signature << " */";
    }
  }
  method_signature << ")";
  return method_signature.str();
}

void api_generator::generate_interface_file(const fc::path& api_interface_filename)
{
  std::ofstream interface_file(api_interface_filename.string());
  interface_file << "#pragma once\n\n";
  interface_file << "namespace bts { namespace api {\n\n";

  interface_file << "  class " << _api_classname << "\n";
  interface_file << "  {\n";
  interface_file << "  public:\n";

  for (const method_description& method : _methods)
  {
    interface_file << "    " << generate_signature_for_method(method, "", true) << " = 0;\n";
  }

  interface_file << "  };\n\n";
  interface_file << "} } // end namespace bts::api\n";
}

void api_generator::generate_client_files(const fc::path& client_header_filename, const fc::path& client_cpp_filename)
{
  std::ofstream header_file(client_header_filename.string());
  std::ofstream cpp_file(client_cpp_filename.string());

  std::string client_classname = _api_classname + "_client";

  header_file << "#pragma once\n\n";
  header_file << "namespace bts { namespace rpc {\n\n";
  header_file << "  class " << client_classname << " : public bts::api::" << _api_classname << "\n";
  header_file << "  {\n";
  header_file << "  public:\n";

  for (const method_description& method : _methods)
  {
    header_file << "    " << generate_signature_for_method(method, "", true) << " override;\n";
  }

  header_file << "  };\n\n";
  header_file << "} } // end namespace bts::rpc\n";


  cpp_file << "#include <header_file>\n";
  cpp_file << "namespace bts { namespace rpc {\n\n";

  for (const method_description& method : _methods)
  {
    cpp_file << generate_signature_for_method(method, client_classname, false) << "\n";
    cpp_file << "{\n";

    if (std::dynamic_pointer_cast<void_type_mapping>(method.return_type))    
      cpp_file << "  _json_connection->async_call(";
    else
      cpp_file << "  return _json_connection->call<" << method.return_type->get_cpp_return_type() << ">(";
    cpp_file << "\"" << method.name << "\"";
    for (const parameter_description& parameter : method.parameters)
      cpp_file << ", " << parameter.name;
    cpp_file << ")";
    if (std::dynamic_pointer_cast<void_type_mapping>(method.return_type))    
      cpp_file << ".wait()";
    cpp_file << ";\n";
    cpp_file << "}\n";
  }
  cpp_file << "\n";
  cpp_file << "} } // end namespace bts::rpc\n";
}

void api_generator::generate_server_files(const fc::path& client_header_filename, const fc::path& client_cpp_filename)
{
  std::ofstream header_file(client_header_filename.string());
  std::ofstream cpp_file(client_cpp_filename.string());

  std::string server_classname = _api_classname + "_server";

  header_file << "#pragma once\n\n";
  header_file << "namespace bts { namespace rpc {\n\n";
  header_file << "  class " << server_classname << "\n";
  header_file << "  {\n";
  header_file << "  public:\n";

  for (const method_description& method : _methods)
  {
    header_file << "    fc::variant " << method.name << "_positional(const fc::variants& parameters);\n";
    header_file << "    fc::variant " << method.name << "_named(const fc::variant_object& parameters);\n";
  }

  header_file << "  };\n\n";
  header_file << "} } // end namespace bts::rpc\n";

  cpp_file << "#include <header_file>\n";
  cpp_file << "namespace bts { namespace rpc {\n\n";

  for (const method_description& method : _methods)
  {
    cpp_file << "fc::variant " << server_classname << "::" << method.name << "_positional(const fc::variants& parameters)\n";
    cpp_file << "{\n";
    unsigned parameter_index = 0;
    for (const parameter_description& parameter : method.parameters)
    {
      if (!parameter.default_value)
      {
        // parameter is required
        cpp_file << "  if (parameters.size() <= " << parameter_index << ")\n";
        cpp_file << "    FC_THROW_EXCEPTION(invalid_arg_exception, \"missing required parameter " << (parameter_index + 1) << " (" << parameter.name << ")\");\n";
        cpp_file << "  " << parameter.type->get_cpp_return_type() << " " << parameter.name << 
                    " = parameters[" << parameter_index << "].as<" << parameter.type->get_cpp_return_type() << ">();\n";
      }
      ++parameter_index;
    }

    cpp_file << "  ";
    if (!std::dynamic_pointer_cast<void_type_mapping>(method.return_type))
      cpp_file << "return";
    cpp_file << "_client->" << method.name << "(";
    bool first_parameter = true;
    for (const parameter_description& parameter : method.parameters)
    {
      if (first_parameter)
        first_parameter = false;
      else
        cpp_file << ", ";
      cpp_file << parameter.name;
    }
    cpp_file << ");\n";
    cpp_file << "}\n";
  }
  cpp_file << "\n";
  cpp_file << "} } // end namespace bts::rpc\n";
}

void api_generator::initialize_type_map_with_fundamental_types()
{
  const char* pass_by_value_types[] =
  {
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t"
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
}


int main(int argc, char*argv[])
{
  // parse command-line options
  boost::program_options::options_description option_config("Allowed options");
  option_config.add_options()("help",                                                             "display this help message")
                             ("api-description",    boost::program_options::value<std::string>(), "API description file name in JSON format")
                             ("api-interface-file", boost::program_options::value<std::string>(), "Filename for the generated C++ pure interface class for this API")
                             ("api-classname",      boost::program_options::value<std::string>(), "Name for the generated C++ classes")
                             ("rpc-client-cpp",     boost::program_options::value<std::string>(), "RPC client source output file name")
                             ("rpc-client-hpp",     boost::program_options::value<std::string>(), "RPC client header output file name")
                             ("rpc-server-cpp",     boost::program_options::value<std::string>(), "RPC server source output file name")
                             ("rpc-server-hpp",     boost::program_options::value<std::string>(), "RPC server header output file name");

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

  generator.load_api_description(option_variables["api-description"].as<std::string>());

  if (option_variables.count("api-interface-file"))
  {
    generator.generate_interface_file(option_variables["api-interface-file"].as<std::string>());
  }

  if (option_variables.count("rpc-client-cpp") ||
      option_variables.count("rpc-client-hpp"))
  {
    if (!option_variables.count("rpc-client-cpp") || !option_variables.count("rpc-client-hpp"))
    {
      std::cout << "You must specify both --rpc-client-cpp and --rpc-client-hpp\n";
      return 1;
    }
    generator.generate_client_files(option_variables["rpc-client-hpp"].as<std::string>(), 
                                    option_variables["rpc-client-cpp"].as<std::string>());
  }

  if (option_variables.count("rpc-server-cpp") ||
      option_variables.count("rpc-server-hpp"))
  {
    if (!option_variables.count("rpc-server-cpp") || !option_variables.count("rpc-server-hpp"))
    {
      std::cout << "You must specify both --rpc-server-cpp and --rpc-server-hpp\n";
      return 1;
    }
    generator.generate_server_files(option_variables["rpc-server-hpp"].as<std::string>(), 
                                    option_variables["rpc-server-cpp"].as<std::string>());
  }

  return 0;
}