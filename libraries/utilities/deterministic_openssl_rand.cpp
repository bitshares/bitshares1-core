#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/time.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>
#include <algorithm>
#include <fc/crypto/sha512.hpp>

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <openssl/rand.h>

// These don't need to do anything if you don't have anything for them to do.
static void deterministic_rand_cleanup() {}
static void deterministic_rand_add(const void *buf, int num, double add_entropy) {}
static int  deterministic_rand_status() { return 1; }
static void deterministic_rand_seed(const void *buf, int num) {}

static fc::sha512 seed;

static int  deterministic_rand_bytes(unsigned char *buf, int num)
{
  std::cout << "deterministic_rand_bytes()\n";
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
    RAND_set_rand_method(&deterministic_rand_vtable);
    seed = new_seed;
    return;
}

} }
