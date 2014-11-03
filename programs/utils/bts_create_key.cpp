#include <bts/blockchain/address.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>
#include<fc/variant_object.hpp>

#include <iostream>

using namespace bts::blockchain;

int main( int argc, char** argv )
{

    if( argc == 1 )
    {
        auto key = fc::ecc::private_key::generate();
        auto obj = fc::mutable_variant_object();

        obj["public_key"] = public_key_type(key.get_public_key());
        obj["private_key"] = key.get_secret();
        obj["wif_private_key"] = bts::utilities::key_to_wif(key);
        obj["native_address"] = bts::blockchain::address(key.get_public_key());
        obj["pts_address"] = bts::blockchain::pts_address(key.get_public_key());

        std::cout << fc::json::to_pretty_string(obj);
        return 0;
    }
    else if (argc == 2)
    {
        std::cout << "writing new private key to JSON file " << argv[1] << "\n";
        fc::ecc::private_key key(fc::ecc::private_key::generate());
        fc::json::save_to_file(key, fc::path(argv[1]));

        std::cout << "bts address: "
        << std::string(bts::blockchain::address(key.get_public_key())) << "\n";

        return 0;
    }
}
