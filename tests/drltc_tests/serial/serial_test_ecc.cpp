
#include <fc/crypto/elliptic.hpp>
#include <fc/io/iostream.hpp>

#include "serial_tester.hpp"
#include "serial_test_macros.hpp"

void serial_test_gen_privkey(fc::ostream* unknown_output)
{
    BEGIN_SERIAL_TEST("gen_privkey");

    fc::ecc::private_key private_key_0 = fc::ecc::private_key::generate();
    fc::ecc::private_key private_key_1 = fc::ecc::private_key::generate();
    fc::ecc::private_key private_key_2 = fc::ecc::private_key::generate();
    fc::ecc::private_key private_key_3 = fc::ecc::private_key::generate();
    
    tester("private_key_0", private_key_0);
    tester("private_key_1", private_key_1);
    tester("private_key_2", private_key_2);
    tester("private_key_3", private_key_3);
    
    return;
}
