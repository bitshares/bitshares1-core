#include <bts/blockchain/momentum.hpp>
#include <fc/thread/thread.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha1.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/crypto/aes.hpp>

#include <unordered_map>
#include <fc/reflect/variant.hpp>
#include <fc/time.hpp>
#include <algorithm>
#include <array>

#include <fc/log/logger.hpp>


namespace bts { namespace blockchain {

   #define SEARCH_SPACE_BITS 50
   #define BIRTHDAYS_PER_HASH 8

   // The Momentum duplicate detector filter
   #define FILTER_SLOTS_POWER 19  /* 2^20 bits - fits in L2 */
   #define FILTER_SIZE_BYTES (1 << (FILTER_SLOTS_POWER+1-3))
   #define PARTITION_BITS     10 /* Balance TLB pressure vs filter */

   #define HASH_MASK ((1ULL<<(64-MOMENTUM_NONCE_BITS))-1)  /* How hash is stored in hashStore */
   #define MOMENTUM_COLHASH_SIZE 36 /* bytes */


   /*
    * Duplicate filter functions.  The duplicate filter is a saturating
    * unary counter with overflow detection:  On addition, the counter
    * for a particular item goes to 1.  Upon a second addition, it goes
     * to 11, and remains there thereafter.  is_in_filter_twice(item)
    * checks the high bit of the item's counter to determine if an item
    * was added more than once to the filter.
    */

   void set_or_double(uint32_t *filter, uint32_t whichbit)
   {
      uint32_t whichword = whichbit/16;
      uint32_t old = filter[whichword];
      uint32_t bitpat = 1UL << (2*(whichbit%16));
      /* no conditional: set 2nd bit of pair if 1st already set, 1st otherwise */
      filter[whichword] = old | (bitpat + (old&bitpat));
   }

   inline void add_to_filter(uint32_t *filter, const uint64_t hash)
   {
      uint32_t whichbit = (uint32_t(hash) & ((1UL<<FILTER_SLOTS_POWER)-1));
      set_or_double(filter, whichbit);
   }

   inline bool is_in_filter_twice(const uint32_t *filter, const uint64_t hash)
   {
      uint32_t whichbit = (uint32_t(hash) & ((1UL<<FILTER_SLOTS_POWER)-1));
      uint32_t cbits = filter[whichbit/16];
      return (cbits & (2UL<<(((whichbit&0xf)<<1))));
   }

   uint32_t *allocate_filter() 
   {
      uint32_t *filter = (uint32_t *)malloc(FILTER_SIZE_BYTES);
      if (!filter) {
	 printf("Error allocating memory for miner\n");
	 return NULL;
      }
      return filter;
   }

   void free_filter(uint32_t *filter)
   {
      free(filter);
   }

   void reset_filter(uint32_t *filter)
   {
      memset(filter, 0x00, FILTER_SIZE_BYTES);
   }

   /* The search procedure operates by generating all 2^26 momentum hash
    * values and storing them in a big table partitioned by 10 bits of
    * the hash.  It then detects duplicates within each partition.
    * The partitions go into a large chunk of memory, with a carefully-sized
    * gap between partitions, calculated so that the probability of
    * overrunning a partition is incredibly small.
    */

   uint32_t partition_offset(uint32_t partition_id)
   {
      uint32_t partition_real_size = 1<<(MOMENTUM_NONCE_BITS-PARTITION_BITS);
      /* Leave a 3.125% space buffer.  This is safe with enough 9s of
       * probability to not matter.  Running over just gives bogus results
       * that are ignored during revalidation.  There is a larger gap
       * left at the end to ensure safety. */

      uint32_t partition_safe_size = partition_real_size + (partition_real_size>>5);
      return partition_id*partition_safe_size;
   }


   /* Once a hash is placed in its partition bucket, the 26 high-order bits are
    * replaced with its momentum nonce number.  Of those 26 bits, 14 were unused
    * (64 bits - 50 momentum bits).  10 of those 14 were used to determine the
    * partition identifier (2^10 partitions).  4 are lost, which increases the
    * number of false collisions that must be re-validated, but it's not large. */

   inline void put_hash_in_bucket(uint64_t hash, uint64_t *hashStore, uint32_t *hashCounts, uint32_t nonce)
   {
      uint32_t bin = hash & ((1<<PARTITION_BITS)-1);
      uint64_t hashval = ((uint64_t(nonce) << (64-MOMENTUM_NONCE_BITS)) | (hash >> (MOMENTUM_NONCE_BITS - (64 - SEARCH_SPACE_BITS)))); /* High-order bits now nonce */
      hashStore[hashCounts[bin]] = hashval;
      hashCounts[bin]++;
   }


   void generate_hashes(pow_seed_type head, uint64_t *hashStore, uint32_t *hashCounts)
   {
      fc::sha512::encoder enc;
      for ( uint32_t n = 0; n < MAX_MOMENTUM_NONCE; n += (BIRTHDAYS_PER_HASH)) {
         enc.write( (char*)&n, sizeof(n));
         enc.write( (char *)&head, sizeof(head));
	 auto result = enc.result();

	 for (uint32_t i = 0; i < BIRTHDAYS_PER_HASH; i++) {
	    put_hash_in_bucket((result._hash[i] >> (64 - SEARCH_SPACE_BITS)), hashStore, hashCounts, n+i);
	 }
	 enc.reset();
      }
   }


  /* Find duplicated hash values within a single partition.
   * Validates that they are actual momentum 50 bit duplicates,
   * and, if so, adds them to the list of results */

   void find_duplicates(uint64_t *hashStore, uint32_t count, std::vector< std::pair<uint32_t,uint32_t> > &results, uint32_t *filter, pow_seed_type head) 
   {

      /* Three passes through the counting filter using different bits of the hash is
       * sufficient to eliminate nearly all false duplicates. */
     for (int pass = 0; pass < 3; pass++) {
        uint32_t shiftby = 0;
        if (pass == 1) shiftby = 19;
        if (pass == 2) shiftby = 9;

        reset_filter(filter);
        for (uint32_t i = 0; i < count; i++) {
           add_to_filter(filter, hashStore[i] >> shiftby);
        }
          
        uint32_t valid_entries = 0;

        for (uint32_t i = 0; i < count; i++) {
           if (is_in_filter_twice(filter, hashStore[i] >> shiftby)) {
              hashStore[valid_entries] = hashStore[i];
              valid_entries++;
           }
        }
        count = valid_entries;
     }

     /* At this point, there are typically 0 - but sometimes 2, 4, or 6 - items
      * remaining in the hashStore for this partition.  Identify the duplicates
      * and pass them in for hash revalidation.  This is an n^2 search, but the
      * number of items is so small it doesn't matter. */
     for (uint32_t i = 0; i < count; i++) {
        if (hashStore[i] == 0) { continue; }
        for (uint32_t j = i+1; j < count; j++) {
           if ((hashStore[i] & HASH_MASK) == (hashStore[j] & HASH_MASK)) {
              uint32_t nonce1 = hashStore[i] >> (64 - MOMENTUM_NONCE_BITS);
              uint32_t nonce2 = hashStore[j] >> (64 - MOMENTUM_NONCE_BITS);
              if (momentum_verify(head, nonce1, nonce2)) {
                 /* Collisions can be used in both directions */
                 results.push_back( std::make_pair( nonce1, nonce2 ) );
                 results.push_back( std::make_pair( nonce2, nonce1 ) );
              }
           }
        }
     }
   }

   
   std::vector< std::pair<uint32_t,uint32_t> > momentum_search( pow_seed_type head )
   {
      std::vector< std::pair<uint32_t,uint32_t> > results;

      uint32_t *filter = allocate_filter();
      if (!filter) {
            printf("Could not allocate filter for mining\n");
            return results;
      }
      uint32_t hashStoreSize = MAX_MOMENTUM_NONCE * sizeof(uint64_t);
      /* inter-partition margin of 3% plus a little extra at the end for
       * paranoia.  Missing things is OK 1 in a billion times, but 
       * crashing isn't, so the extra 1/64th at the end pushes the
       * probability of overrun down into the infestisimally small range.
       */

      hashStoreSize += ((hashStoreSize >> 5) + (hashStoreSize >> 6));
      uint64_t *hashStore = (uint64_t *)malloc(hashStoreSize);
      if (!hashStore) {
            free_filter(filter);
            printf("Could not allocate hashStore for mining\n");
            return results;
      }

      static const int NUM_PARTITIONS = (1<<PARTITION_BITS);
      uint32_t hashCounts[NUM_PARTITIONS];

      for (int i = 0; i < NUM_PARTITIONS; i++) { 
            hashCounts[i] = partition_offset(i);
      }

      generate_hashes(head, hashStore, hashCounts);
      for (uint32_t i = 0; i < NUM_PARTITIONS; i++) {
	    int binStart = partition_offset(i);
	    int binCount = hashCounts[i] - binStart;
	    find_duplicates(hashStore+binStart, binCount, results, filter, head);
      }

      free_filter(filter);
      free(hashStore);

      return results;
   }


   bool momentum_verify( pow_seed_type head, uint32_t a, uint32_t b )
   {
       if( a == b ) return false;
       if( a > MAX_MOMENTUM_NONCE ) return false;
       if( b > MAX_MOMENTUM_NONCE ) return false;

       uint32_t ia = (a / 8) * 8; 
       fc::sha512::encoder enca;
       enca.write( (char*)&ia, sizeof(ia) );
       enca.write( (char*)&head, sizeof(head) );
       auto ar = enca.result();

       uint32_t ib = (b / 8) * 8; 
       fc::sha512::encoder encb;
       encb.write( (char*)&ib, sizeof(ib) );
       encb.write( (char*)&head, sizeof(head) );
       auto br = encb.result();

       return (ar._hash[a%8]>>14) == (br._hash[b%8]>>14);
   }

} } // bts::blockchain
