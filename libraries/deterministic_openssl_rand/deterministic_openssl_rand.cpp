#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/time.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>
#include <algorithm>
#include <fc/crypto/sha512.hpp>
#include <bts/blockchain/config.hpp>

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <openssl/rand.h>

static bool deterministic_rand_warning_shown = false;

static void _warn()
{
  if (!deterministic_rand_warning_shown)
  {
    std::cerr << "********************************************************************************\n"
              << "DETERMINISTIC RANDOM NUMBER GENERATION ENABLED\n"
              << "********************************************************************************\n"
              << "TESTING PURPOSES ONLY -- NOT SUITABLE FOR PRODUCTION USE\n"
              << "DO NOT USE PRIVATE KEYS GENERATED WITH THIS PROGRAM FOR LIVE FUNDS\n"
              << "********************************************************************************\n";
    deterministic_rand_warning_shown = true;
  }
#ifndef BTS_TEST_NETWORK
  std::cerr << "This program looks like a production application, but is calling the deterministic RNG.\n"
            << "Perhaps the compile-time options in config.hpp were misconfigured?\n";
  exit(1);
#else
  return;
#endif
}

// These don't need to do anything if you don't have anything for them to do.
static void deterministic_rand_cleanup() { _warn(); }
static void deterministic_rand_add(const void *buf, int num, double add_entropy) { _warn(); }
static int  deterministic_rand_status() { _warn(); return 1; }
static void deterministic_rand_seed(const void *buf, int num) { _warn(); }

static fc::sha512 seed;

static int  deterministic_rand_bytes(unsigned char *buf, int num)
{
  _warn();
  while (num)
  {
    seed = fc::sha512::hash(seed);

    int bytes_to_copy = std::min<int>(num, sizeof(seed));
    memcpy(buf, &seed, bytes_to_copy);
    num -= bytes_to_copy;
    buf += bytes_to_copy;
  }
  return 1;
}

// Create the table that will link OpenSSL's rand API to our functions.
static RAND_METHOD deterministic_rand_vtable = {
        deterministic_rand_seed,
        deterministic_rand_bytes,
        deterministic_rand_cleanup,
        deterministic_rand_add,
        deterministic_rand_bytes,
        deterministic_rand_status
};

namespace bts { namespace utilities {

void set_random_seed_for_testing(const fc::sha512& new_seed)
{
    _warn();
    RAND_set_rand_method(&deterministic_rand_vtable);
    seed = new_seed;
    return;
}

} }
