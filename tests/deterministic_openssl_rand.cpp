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
static void stdlib_rand_cleanup() {}
static void stdlib_rand_add(const void *buf, int num, double add_entropy) {}
static int stdlib_rand_status() { return 1; }
static void stdlib_rand_seed(const void *buf, int num) {}

static fc::sha512 seed;
static int stdlib_rand_bytes(unsigned char *buf, int num)
{

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
RAND_METHOD stdlib_rand_meth = {
        stdlib_rand_seed,
        stdlib_rand_bytes,
        stdlib_rand_cleanup,
        stdlib_rand_add,
        stdlib_rand_bytes,
        stdlib_rand_status
};

// This is a public-scope accessor method for our table.
RAND_METHOD *RAND_stdlib() { return &stdlib_rand_meth; }

void set_random_seed_for_testing(const fc::sha512& new_seed)
{
  RAND_set_rand_method(RAND_stdlib());
  seed = new_seed;
}
