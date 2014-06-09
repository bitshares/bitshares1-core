#pragma once
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/reflect/reflect.hpp>
#include <string>

namespace bts {namespace keyhotee
{


   /**
    *  By salting your the seed with easy-to-remember personal
    *  information, it enhances the security of the password from
    *  bruteforce, untargeted, attacks.  Someone specifically
    *  targeting you could find out this information and then
    *  attempt to attack your password.  
    *
    *  @note all of this information, save your password, is
    *        on your credit report.  You should still pick
    *        a reasonable length, uncommon password. We also
    *        cache this information on disk encrypted with just
    *        password so you don't have to enter it all of the
    *        time.   If your computer is compromised, atacks
    *        will become easier.
    */
   struct profile_config
   {
   
       std::string firstname;
       std::string middlename;
       std::string lastname;
       std::string brainkey;
   };

   fc::ecc::private_key import_keyhotee_id( const profile_config& cfg, const std::string& keyhoteeid );
} } // bts::keyhotee

FC_REFLECT( bts::keyhotee::profile_config, (firstname)(middlename)(lastname)(brainkey) )
