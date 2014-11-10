
#pragma once

#include <string>

#include <fc/io/fstream.hpp>
#include <fc/crypto/sha512.hpp>

#include "deterministic_openssl_rand.hpp"

// BEGIN_SERIAL_TEST does the following:
// create tester object
// initialize openssl rand
// add expected results from file
#define BEGIN_SERIAL_TEST(test_name)                                   \
     fc::string expected_result_filename(test_name);                   \
     serial_tester tester(expected_result_filename, unknown_output);   \
     expected_result_filename = "results/" + expected_result_filename; \
     expected_result_filename += ".txt";                               \
                                                                       \
     fc::sha512 _hash_test_name = fc::sha512::hash( test_name,         \
                    std::char_traits<char>::length(test_name) );       \
     set_random_seed_for_testing(_hash_test_name);                     \
                                                                       \
     {                                                                 \
        fc::ifstream expected_result_file(expected_result_filename);   \
        tester.add_expected_results(expected_result_file);             \
     }
