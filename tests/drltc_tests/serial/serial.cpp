
#include <iostream>
#include <map>

#include <fc/crypto/elliptic.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/iostream.hpp>
#include <fc/io/sstream.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/wallet/wallet.hpp>

#include "serial_tester.hpp"
#include "serial_tests.hpp"

using namespace std;

int main(int argc, char **argv, char **envp)
{
    try{

	fc::ofstream unknown_output("unknown-serial.txt");
	
	serial_test_gen_privkey(&unknown_output);

    } FC_LOG_AND_RETHROW();

    cout << "done\n";

    return 0;
}
