
#include <boost/program_options.hpp>

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/utilities/deterministic_openssl_rand.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>
#include <fc/variant_object.hpp>

#include <iostream>

using namespace bts::blockchain;

boost::program_options::variables_map parse_option_variables(int argc, char** argv)
{
    boost::program_options::positional_options_description p_option_config;
    p_option_config.add("json-outfile", 1);

    boost::program_options::options_description option_config("Usage");
    option_config.add_options()
        ("help", "Display this help message and exit")

        ("json-outfile", boost::program_options::value<string>()->default_value(""), "Output file for private key data")

        ("seed", boost::program_options::value<string>(), "Set seed for deterministic key generation")
        ("count", boost::program_options::value<int64_t>()->default_value(-1), "Set number of keys to generate")
        ;

    boost::program_options::variables_map option_variables;
    try
    {
        boost::program_options::store(
                                      boost::program_options::command_line_parser(argc, argv)
                                     .options(option_config)
                                     .positional(p_option_config)
                                     .run(),
                                     option_variables
                                     );
        boost::program_options::notify(option_variables);
    }
    catch (boost::program_options::error& cmdline_error)
    {
        std::cerr << "Error: " << cmdline_error.what() << "\n";
        std::cerr << option_config << "\n";
        exit(1);
    }

    if (option_variables.count("help"))
    {
        std::cout << option_config << "\n";
        exit(0);
    }

    return option_variables;
}

int run( int64_t index, fc::optional<string> seed, fc::optional<fc::path> json_outfile )
{
     fc::ecc::private_key key;
    
     if( seed.valid() )
     {
         string effective_seed;
         if( index >= 0 )
             effective_seed = (*seed)+std::to_string(index);
         else
             effective_seed = (*seed);
         fc::sha256 hash_effective_seed = fc::sha256::hash(
             effective_seed.c_str(),
             effective_seed.length() );
         key = fc::ecc::private_key::regenerate( hash_effective_seed );
    }
    else
        key = fc::ecc::private_key::generate();
    
    
    if( !json_outfile.valid() )
    {
        auto obj = fc::mutable_variant_object();

        obj["public_key"] = public_key_type(key.get_public_key());
        obj["private_key"] = key.get_secret();
        obj["wif_private_key"] = bts::utilities::key_to_wif(key);
        obj["native_address"] = bts::blockchain::address(key.get_public_key());
        obj["pts_address"] = bts::blockchain::pts_address(key.get_public_key());

        std::cout << fc::json::to_pretty_string(obj) << '\n';
        return 0;
    }
    else
    {
        std::cout << "writing new private key to JSON file " << (*json_outfile).string() << "\n";
        fc::json::save_to_file(key, (*json_outfile));

        std::cout << "bts address: "
        << std::string(bts::blockchain::address(key.get_public_key())) << "\n";

        return 0;
    }
}

int main( int argc, char** argv )
{
    boost::program_options::variables_map option_variables =
        parse_option_variables(argc, argv);

    fc::optional<string> seed;
    fc::optional<fc::path> json_outfile;
    
    if( option_variables.count("seed") )
        seed = option_variables["seed"].as<string>();
    if( option_variables.count("json-outfile") )
    {
        string s_json_outfile = option_variables["json-outfile"].as<string>();
        if( s_json_outfile != "" )
            json_outfile = fc::path(s_json_outfile);
    }
    
    if( ! option_variables.count("count") )
    {
        std::cerr << "count undefined, should never happen!\n";
        return 1;
    }
    
    int n = option_variables["count"].as<int64_t>();
    if( n != -1 )
    {
        if( json_outfile.valid() )
        {
            std::cerr << "Combining --count and --json-outfile is currently unsupported\n";
            return 1;
        }
        std::cout << "[\n";
        for( int64_t i=0; i<n; i++ )
        {
            if (i != 0)
                std::cout << ",\n";
            int result = run(i, seed, json_outfile);
            if( result != 0 )
                return result;
        }
        std::cout << "]\n";
    }
    else
        return run(-1, seed, json_outfile);
    
    return 0;
}
