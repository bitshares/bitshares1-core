#define BOOST_TEST_MODULE SignatureTest
#include <boost/test/unit_test.hpp>

#include <fc/crypto/elliptic.hpp>
#include "deterministic_openssl_rand.hpp"

#include <fc/crypto/sha256.hpp>
#include <fc/time.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>
#include <algorithm>
#include <fc/crypto/sha512.hpp>

BOOST_AUTO_TEST_CASE(ecc_signatures)
{
  std::string plaintext("Here is some text to sign");
  fc::sha256::encoder sha_encoder;
  sha_encoder.write(plaintext.c_str(), plaintext.size());
  fc::sha256 hash_of_plaintext = sha_encoder.result();

  // code for generating a key:
  //fc::ecc::private_key signing_key = fc::ecc::private_key::generate();
  //std::string private_key_as_string = fc::variant(signing_key).as_string();
  //fc::ecc::private_key reconstructed_private_key = fc::variant(private_key_as_string).as<fc::ecc::private_key>();
  //assert(signing_key == reconstructed_private_key);
  fc::ecc::private_key signing_key = fc::variant("940ff36f9c6d4d19d8c965bfbbe50bd318774a2af3d8c76021e681af462f943e").as<fc::ecc::private_key>();

  // generate two signatures before injecting our code into openssl    
  fc::ecc::signature first_nondeterministic_signature = signing_key.sign(hash_of_plaintext);
  fc::ecc::signature second_nondeterministic_signature = signing_key.sign(hash_of_plaintext);
  BOOST_REQUIRE(first_nondeterministic_signature != second_nondeterministic_signature);

  // set our fake random number generator, generate two signatures
  set_random_seed_for_testing(fc::sha512());

  fc::ecc::signature first_deterministic_signature = signing_key.sign(hash_of_plaintext);
  fc::ecc::signature second_deterministic_signature = signing_key.sign(hash_of_plaintext);
  BOOST_REQUIRE(first_deterministic_signature != second_deterministic_signature);
  BOOST_REQUIRE(first_deterministic_signature != first_nondeterministic_signature);
  BOOST_REQUIRE(second_deterministic_signature != second_nondeterministic_signature);

  set_random_seed_for_testing(fc::sha512());
  fc::ecc::signature third_deterministic_signature = signing_key.sign(hash_of_plaintext);
  fc::ecc::signature fourth_deterministic_signature = signing_key.sign(hash_of_plaintext);
  BOOST_REQUIRE(third_deterministic_signature == first_deterministic_signature);
  BOOST_REQUIRE(fourth_deterministic_signature == second_deterministic_signature);
}