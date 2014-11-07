#include <bts/blockchain/address.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>
#include <fc/variant_object.hpp>
#include <fc/exception/exception.hpp>

#include <iostream>


using namespace bts::blockchain;

int main( int argc, char** argv )
{

    if( argc == 2 )
    {
        fc::ecc::private_key k;
        try{
            auto key = bts::blockchain::public_key_type(std::string(argv[1]));
            auto obj = fc::mutable_variant_object();

            obj["public_key"] = public_key_type(key);
            obj["native_address"] = bts::blockchain::address(key);
            obj["pts_address"] = bts::blockchain::pts_address(key);

            std::cout << fc::json::to_pretty_string(obj);
            return 0;

        } catch (fc::exception e) {
            std::cout << e.to_detail_string();
        }
    }
    return -1;
}
