#include <fstream>
#include <algorithm>

#include <boost/locale.hpp>

#include <bts/bitcoin/multibit.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/scrypt.hpp>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace bitcoin {

static unsigned long read_protobuf_var_int( std::ifstream &f )
{
  unsigned long l = 0;
  for(;;)
  {
     unsigned char ch;
     f.read( (char*)&ch, 1 );
     l = (l << 7) | (ch & 0x7f);
     if( !(ch&0x80) )
        break;
  }
  return l;
}

static void decrypt_key ( const std::string& passphrase, const std::vector<unsigned char> &salt,
                         unsigned long n, unsigned long r, unsigned long p,
                         std::vector<unsigned char> &ekey, std::vector<fc::ecc::private_key> &imported_keys)
{

  try {
     std::vector<unsigned char> dk(32);
     std::basic_string<char16_t> utf16 = boost::locale::conv::utf_to_utf<char16_t>(passphrase);
     std::vector<unsigned char> passbytes(utf16.size() * 2);
     for (unsigned int i=0; i<utf16.size(); i++)
     {
        passbytes[2*i] = (utf16[i] & 0xff00) >> 8;
        passbytes[2*i+1] = utf16[i] & 0x00ff;
     }

     fc::scrypt_derive_key( passbytes, salt, n, r, p, dk );

     std::vector<unsigned char> plaindata(ekey.size()-20);

     if( fc::aes_decrypt( &ekey[20], ekey.size()-20, &dk[0], &ekey[2], &plaindata[0] ) >= 32 )
     {
        fc::sha256 hash( fc::to_hex( (char*)&plaindata[0], 32 ) );
        fc::ecc::private_key privkey = fc::ecc::private_key::regenerate( hash );
        imported_keys.push_back( privkey );
     }
  }
  catch ( const fc::exception& e )
  {
     wlog( "${e}", ("e",e.to_detail_string()) );
  }
}

static std::vector<fc::ecc::private_key> _import_multibit_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{

  std::vector<fc::ecc::private_key> imported_keys;
  std::vector<std::vector<unsigned char>> encrypted_keys;

  // multibit wallets use protobuf
  std::ifstream wallet( wallet_dat.to_native_ansi_path().c_str(), std::fstream::in );
  long keyend = 0;

  std::vector<unsigned char> scrypt_salt;
  long scrypt_n = 16384;
  long scrypt_r = 8;
  long scrypt_p = 1;

  while( !wallet.eof() ) {
     unsigned long key = read_protobuf_var_int( wallet );
     if( wallet.eof() )
        break;

     unsigned long index = key >> 3;
     unsigned long type = (key & 0x7);

     if( type == 0 )
     {
        // skip varint
        (void)read_protobuf_var_int( wallet );
     }
     else if( type == 1 )
     {
        // skip 64 bits
        wallet.seekg( 8, std::ios_base::cur );
     }
     else if( type == 5 )
     {
        // skip 32 bits
        wallet.seekg( 4, std::ios_base::cur );
     }
     else if( type == 2 )
     {
        unsigned long len = read_protobuf_var_int( wallet );
        if( index == 3 && wallet.tellg() >= keyend )
        {
           keyend = wallet.tellg();
           keyend += len;
        }
        else if( index == 2 && wallet.tellg() < keyend )
        {
           if( len == 32 )
           {
              std::vector<char> privkeydata(32);
              wallet.read( &privkeydata[0], 32 );
              fc::sha256 hash( fc::to_hex( &privkeydata[0], 32 ) );
              fc::ecc::private_key privkey = fc::ecc::private_key::regenerate( hash );
              imported_keys.push_back( privkey );
           }
           else
           {
              wallet.seekg( len, std::ios_base::cur );
           }
        }
        else if( index == 6 )
        {
           if( wallet.tellg() < keyend )
           {
              // encrypted key
              std::vector<unsigned char> privkeydata(len);
              wallet.read((char*)&privkeydata[0], len );
              encrypted_keys.push_back( privkeydata );
           }
           else
           {
              // scrypt parameters
              long scryptend = wallet.tellg();
              scryptend += read_protobuf_var_int( wallet );
              unsigned long saltlen = read_protobuf_var_int( wallet );
              std::vector<unsigned char> salt(saltlen);
              wallet.read( (char*)&salt[0], saltlen );
              scrypt_salt = salt;
              while( wallet.tellg() < scryptend )
              {
                 key = read_protobuf_var_int( wallet );
                 index = key >> 3;
                 type = (key & 0x7);
                 if( type != 0 || index < 1 || index > 4 )
                    break;
                 if( index == 2 )
                 {
                    scrypt_n = read_protobuf_var_int( wallet );
                 }
                 else if( index == 3 )
                 {
                    scrypt_r = read_protobuf_var_int( wallet );
                 }
                 else if( index == 4 )
                 {
                    scrypt_p = read_protobuf_var_int( wallet );
                 }
              }
           }
        }
        else
        {
           wallet.seekg( len, std::ios_base::cur );
        }
     }
  }

  wallet.close();

  for( auto &ekey : encrypted_keys )
  {
     decrypt_key( passphrase, scrypt_salt, scrypt_n, scrypt_r, scrypt_p, ekey, imported_keys );
  }

  return imported_keys;
}

std::vector<fc::ecc::private_key> import_multibit_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{ try {
     if( !fc::exists( wallet_dat ) )
        FC_CAPTURE_AND_THROW( fc::file_not_found_exception, (wallet_dat) );
     return _import_multibit_wallet( wallet_dat, passphrase );
} FC_CAPTURE_AND_RETHROW( (wallet_dat) ) }

} } // bts::bitcoin
