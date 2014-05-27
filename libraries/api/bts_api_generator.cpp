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

struct parameter
{
  std::string name;
  type_mapping_ptr type;
  std::string description;
  fc::optional<std::string> default_value;
  fc::optional<std::string> example;
};
typedef std::list<parameter> parameter_list;

struct method
{
  std::string name;
  type_mapping_ptr return_type;
  parameter_list parameters;
};
typedef std::list<method> method_list;

class api_generator
{
private:
  std::string _api_classname;

  typedef std::map<std::string, type_mapping_ptr> type_map_type;
  type_map_type _type_map;

  method_list _methods;
public:
  api_generator(const std::string& classname); 

  void load_api_description(const fc::path& api_description_file);
  void generate_interface_file(const fc::path& api_description_file);
private:
  type_mapping_ptr lookup_type_mapping(const std::string& type_string);
  void initialize_type_map_with_fundamental_types();
  void load_type_map(const fc::variants& json_type_map);
  parameter_list load_parameters(const fc::variants& parameter_descriptions);
  void load_method_descriptions(const fc::variants& method_descriptions);
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

parameter_list api_generator::load_parameters(const fc::variants& parameter_descriptions)
{
  parameter_list parameters;
  for (const fc::variant& parameter_description_variant : parameter_descriptions)
  {
    fc::variant_object json_parameter_description = parameter_description_variant.get_object();
    parameter parameter_description;
    FC_ASSERT(json_parameter_description.contains("name"));
    parameter_description.name = json_parameter_description["name"].as_string();
    FC_ASSERT(json_parameter_description.contains("description"));
    parameter_description.description = json_parameter_description["description"].as_string();
    FC_ASSERT(json_parameter_description.contains("type"));
    parameter_description.type = lookup_type_mapping(json_parameter_description["type"].as_string());
    if (json_parameter_description.contains("default_value"))
      parameter_description.default_value = json_parameter_description["default_value"].as_string();
    if (json_parameter_description.contains("example"))
      parameter_description.example = json_parameter_description["example"].as_string();

    parameters.push_back(parameter_description);
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
      method method_info;
      method_info.name = method_name;
      FC_ASSERT(json_method_description.contains("return_type"));
      std::string return_type_name = json_method_description["return_type"].as_string();
      method_info.return_type = lookup_type_mapping(return_type_name);
      
      FC_ASSERT(json_method_description.contains("parameters"));
      method_info.parameters = load_parameters(json_method_description["parameters"].get_array());

      _methods.push_back(method_info);
    }
    FC_RETHROW_EXCEPTIONS(warn, "error encountered parsing method description for method \"${method_name}\"", ("method_name", method_name));
  }
}

void api_generator::load_api_description(const fc::path& api_description_file)
{
  FC_ASSERT(fc::exists(api_description_file));
  fc::variant_object api_description = fc::json::from_file(api_description_file).get_object();
  if (api_description.contains("type_map"))
    load_type_map(api_description["type_map"].get_array());
  FC_ASSERT(api_description.contains("methods"));
  load_method_descriptions(api_description["methods"].get_array());
}

void api_generator::generate_interface_file(const fc::path& api_interface_file)
{
  std::ofstream interface_file(api_interface_file.string());
  interface_file << "#pragma once\n\n";
  interface_file << "namespace bts { namespace rpc {\n";

  interface_file << "class << " << _api_classname << "\n";
  interface_file << "{\n";
  interface_file << "public:\n";

  for (const method& method_description : _methods)
  {
    interface_file << method_description.return_type->get_cpp_return_type() << " " << method_description.name << "(";
    for (const parameter& parameter_description : method_description.name)
    {
      
    }
  }

  interface_file << "}\n\n";
  interface_file << "} } // end namespace bts::rpc\n";
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

  return 0;
}