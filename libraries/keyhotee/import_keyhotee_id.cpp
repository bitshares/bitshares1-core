#include <bts/keyhotee/import_keyhotee_id.hpp>
#include <fc/thread/thread.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace keyhotee {
    class extended_private_key
    {
       public:
          extended_private_key( const fc::sha512& seed );
          extended_private_key( const fc::sha256& key, const fc::sha256& chain_code );
          extended_private_key();
    
          /** @param pub_derivation - if true, then extended_public_key can be used
           *      to calculate child keys, otherwise the extended_private_key is
           *      required to calculate all children.
           */
          extended_private_key child( uint32_t c, bool pub_derivation = false )const;
    
          operator fc::ecc::private_key()const;
          fc::ecc::public_key get_public_key()const;
         
          fc::sha256          priv_key;
          fc::sha256          chain_code;
    };

     extended_private_key::extended_private_key( const fc::sha256& key, const fc::sha256& ccode )
     :priv_key(key),chain_code(ccode)
     {
     }

     extended_private_key::extended_private_key( const fc::sha512& seed )
     {
       memcpy( (char*)&priv_key, (char*)&seed, sizeof(priv_key) );
       memcpy( (char*)&chain_code, ((char*)&seed) + sizeof(priv_key), sizeof(priv_key) );
     }

     extended_private_key::extended_private_key(){}

     extended_private_key extended_private_key::child( uint32_t child_idx, bool pub_derivation )const
     { try {
       extended_private_key child_key;

       fc::sha512::encoder enc;
       if( pub_derivation )
       {
         fc::raw::pack( enc, get_public_key() );
       }
       else
       {
         uint8_t pad = 0;
         fc::raw::pack( enc, pad );
         fc::raw::pack( enc, priv_key );
       }
       fc::raw::pack( enc, child_idx );
       fc::sha512 ikey = enc.result();

       fc::sha256 ikey_left;
       fc::sha256 ikey_right;

       memcpy( (char*)&ikey_left, (char*)&ikey, sizeof(ikey_left) );
       memcpy( (char*)&ikey_right, ((char*)&ikey) + sizeof(ikey_left), sizeof(ikey_right) );

       child_key.priv_key  = fc::ecc::private_key::generate_from_seed( priv_key, ikey_left ).get_secret();
       child_key.chain_code = ikey_right; 

       return child_key;
     } FC_RETHROW_EXCEPTIONS( warn, "child index ${child_idx}", ("child_idx", child_idx ) ) }

     extended_private_key::operator fc::ecc::private_key()const
     {
       return fc::ecc::private_key::regenerate( priv_key );
     }

     fc::ecc::public_key extended_private_key::get_public_key()const
     {
       return fc::ecc::private_key::regenerate( priv_key ).get_public_key();
     }


  /**
   *  This method will take several minutes to run and is designed to
   *  make rainbow tables difficult to calculate.
   */
  fc::sha512 stretch_seed( const fc::sha512& seed )
  {
      fc::thread t("stretch_seed");
	        fc::sha512 stretched_seed = t.async( [=]() {
          std::vector<fc::sha512> seeds(1024*1024*4);
          seeds[0] = seed;
          for( uint32_t i = 1; i < seeds.size(); ++i )
          {
             seeds[i] = fc::sha512::hash((char*)&seeds[i-1], sizeof(fc::sha512) ); 
          }
          auto result = fc::sha512::hash( (char*)&seeds[0], sizeof(fc::sha512)*seeds.size() );
          return result;
      } ).wait();
	  t.quit();
	  return stretched_seed;
  }


   fc::ecc::private_key import_keyhotee_id( const profile_config& cfg, const std::string& keyhoteeid )
   {
       fc::sha512::encoder encoder;
       fc::raw::pack( encoder, cfg );
       auto seed = encoder.result();

       /// note: this could take a minute
       // std::cerr << "start keychain::stretch_seed\n";
       auto stretched_seed   = stretch_seed( seed );

       extended_private_key ext_priv_key(stretched_seed);
       return ext_priv_key.child( fc::hash64(keyhoteeid.c_str(),keyhoteeid.size()),false );
   }
} }  // namespace bts::keyhotee
