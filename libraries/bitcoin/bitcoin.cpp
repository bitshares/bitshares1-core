#include <algorithm>

#include <bts/bitcoin/bitcoin.hpp>

#include <bts/blockchain/pts_address.hpp>

#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/hex.hpp>

#include <db_cxx.h>
#include <openssl/aes.h>
#include <openssl/evp.h>

namespace bts { namespace bitcoin {

// bitcoin wallet DB key / value blob handling
class wallet_db_blob
{
  private:
     std::vector<unsigned char> _blob;
     unsigned int _pos;

  public:
     wallet_db_blob(){}

     void assign( char *data, int len )
     {
        FC_ASSERT( data != NULL );
        _blob.assign( data, data + len );
        _pos = 0;
     }

     unsigned int get_varint()
     {
        FC_ASSERT( _pos + 1 <= _blob.size() );
        unsigned int varint = 0;
        switch( _blob[_pos++] )
        {
           case 0xfd:
              FC_ASSERT( _pos + 2 <= _blob.size() );
              varint = _blob[_pos] | _blob[_pos+1] << 8;
              _pos += 2;
              break;
           case 0xfe:
              varint = get_uint();
              break;
           case 0xff:
              // this simply won't happen - no wallets which requires 64-bit lengths...
              FC_ASSERT( false );
              break;
           default:
              varint = _blob[_pos-1];
              break;
        }
        return varint;
     }

     std::vector<unsigned char> get_data()
     {
        return get_data( get_varint() );
     }

     std::vector<unsigned char> get_data( unsigned int len )
     {
        FC_ASSERT( _pos + len <= _blob.size() );
        std::vector<unsigned char> slice( _blob.begin() + _pos, _blob.begin() + _pos + len );
        _pos += len;
        return slice;
     }

     std::string get_string()
     {
        std::vector<unsigned char> v = get_data();
        std::string s( v.begin(), v.end() );
        return s;
     }

     unsigned int get_uint()
     {
        FC_ASSERT( _pos + 4 <= _blob.size() );
        unsigned int v = _blob[_pos] | _blob[_pos+1] << 8 | _blob[_pos+2] << 16 | _blob[_pos+3] << 24;
        _pos += 4;
        return v;
     }

     unsigned char get_uint8()
     {
        FC_ASSERT( _pos + 1 <= _blob.size() );
        return _blob[_pos++];
     }

     void skip( unsigned int len )
     {
        FC_ASSERT( _pos + len <= _blob.size() );
        _pos += len;
     }
};

// wrapper around Berkeley DB C++ API
class wallet_db
{
  private:
     Db _db;
     Dbc *_cursor;

  public:
     wallet_db( const char *path ):_db(NULL, 0),_cursor(NULL)
     {
        _db.open( NULL, path, "main", DB_BTREE, DB_RDONLY, 0 );
        _db.cursor( NULL, &_cursor, 0 );
     }

     ~wallet_db()
     {
        if( _cursor ) _cursor->close();
        _db.close(0);
     }

     bool getNext( wallet_db_blob &key, wallet_db_blob &value )
     {
        Dbt k, v;
        if( _cursor->get( &k, &v, DB_NEXT)  != 0 )
           return false;
        key.assign( (char*)k.get_data(), k.get_size() );
        value.assign( (char*)v.get_data(), v.get_size() );
        return true;
     }
};

// unencrypted / encrypted keys
struct wallet_key
{
     bool _encrypted;
     std::string _address_compressed;
     std::string _address_uncompressed;
     std::vector<unsigned char> _pubkey;
     fc::ecc::private_key _privkey;
     std::vector<unsigned char> _encprivkey;

     wallet_key( std::vector<unsigned char>& pubkey, fc::ecc::private_key& privkey ):_pubkey(pubkey)
     {
        set_private_key( privkey );
     }

     wallet_key( std::vector<unsigned char>& pubkey, std::vector<unsigned char>& encprivkey )
     :_encrypted(true),_pubkey(pubkey),_encprivkey(encprivkey){}

     void set_private_key( fc::ecc::private_key& privkey )
     {
        _encrypted = false;
        _privkey = privkey;
        bts::blockchain::pts_address a_c( privkey.get_public_key(), true );
        bts::blockchain::pts_address a_uc( privkey.get_public_key(), false );
        _address_compressed = a_c;
        _address_uncompressed = a_uc;
     }
};

static bool decrypt( std::vector<unsigned char>& privkey, std::vector<unsigned char>& key,
                    std::vector<unsigned char> iv, std::vector<unsigned char>& plainkey )
{ try {
  FC_ASSERT( privkey.size() >= 32 );

  std::vector<unsigned char> plaindata(privkey.size());
  if ( fc::aes_decrypt( (unsigned char*)&privkey[0], privkey.size(), &key[0], &iv[0], (unsigned char*)&plaindata[0] ) >= 32 )
  {
     plainkey.assign( (char*)&plaindata[0], (char*)&plaindata[0] + 32 );
     return true;
  }
  FC_ASSERT( false, "Unable to decrypt key" );
} FC_RETHROW_EXCEPTIONS( warn, "unable to decrypt key" ) }

static bool private_key_matches_public( fc::ecc::private_key& privkey, std::vector<unsigned char> &pubkey )
{
  fc::string h1 = fc::to_hex( (char*)&pubkey[0], pubkey.size() );
  fc::string h2;

  if( pubkey[0] < 0x04 )
  {
     // compressed
     fc::ecc::public_key_data pkd = privkey.get_public_key().serialize();
     h2 = fc::to_hex( (char*)&pkd, pkd.size() );
  }
  else
  {
     // not compressed
     fc::ecc::public_key_point_data pkd = privkey.get_public_key().serialize_ecc_point();
     h2 = fc::to_hex( (char*)&pkd, pkd.size() );
  }

  if( h1 == h2 ) return true;
  return false;
}

static std::vector<fc::ecc::private_key> collect_keys(  wallet_db &db, const std::string& passphrase )
{
  std::vector<wallet_key> keys;
  std::vector<std::string> names;
  std::vector<std::vector<unsigned char>> mkeys;
  wallet_db_blob key, value;

  while( db.getNext( key, value ) ) 
  {
        std::string cmd = key.get_string();

        if( cmd == "key" || cmd == "wkey" )
        {
           std::vector<unsigned char> pubkey = key.get_data();
           // both 'key' and 'wkey' start off with the private key blob which is the
           // only thing we are interested in
           // the private key blob is an ASN.1 encoded representation, but we can
           // ignore all the scary ASN.1 stuff and just look for the correct bytes.
           //   privkey is at offset 9 if blob=0x3082...
           //   privkey is at offset 8 if blob=0x3081...
           FC_ASSERT( value.get_varint() > 0x81 ); // skip the blob size
           FC_ASSERT( value.get_uint8() == 0x30 ); // ASN.1 SEQUENCE tag
           unsigned char seqlen = value.get_uint8();
           FC_ASSERT( seqlen == 0x81 || seqlen == 0x82 );
           value.skip( (seqlen == 0x81) ? 6 : 7 );
           std::vector<unsigned char> privkeydata = value.get_data( 32 );
           fc::sha256 hash( fc::to_hex( (char*)&privkeydata[0], 32 ) );
           fc::ecc::private_key privkey = fc::ecc::private_key::regenerate( hash );
           if( private_key_matches_public( privkey, pubkey ) )
           {
              wallet_key key( pubkey, privkey );
              keys.push_back( key );
           }
        }
        else if( cmd == "ckey" )
        {
           std::vector<unsigned char> pubkey = key.get_data();
           std::vector<unsigned char> encprivkey = value.get_data();
           wallet_key key( pubkey, encprivkey );
           keys.push_back( key );
        }
        else if( cmd == "mkey" )
        {
           std::vector<unsigned char> privkey = value.get_data();
           std::vector<unsigned char> salt = value.get_data();
           unsigned int method = value.get_uint();
           unsigned int iters = value.get_uint();
           if( method != 0 )
              continue; // not a known method

           std::vector<unsigned char> key( 32 );
           std::vector<unsigned char> iv( 16 );
           if( EVP_BytesToKey( EVP_aes_256_cbc(), EVP_sha512(),
                               (unsigned char*)&salt[0],
                               (unsigned char*)passphrase.c_str(), passphrase.size(), iters,
                               &key[0], &iv[0] ) != 32 )
           {
              wlog( "error importing key" );
              continue;
           }

           // now get masterkey by decrypting privkey with the key generated above
           std::vector<unsigned char> masterkey;
           if( decrypt( privkey, key, iv, masterkey ) )
           {
              mkeys.push_back( masterkey );
           }
           else
           {
              elog( "error decrypting key" );
           }
        }
        else if( cmd == "name" )
        {
           std::string b58name = key.get_string();
           names.push_back( b58name );
        }
  } // while...

  std::vector<fc::ecc::private_key> ekeys;

  // decrypt encrypted keys
  for( auto &key : keys )
  {
     if( key._encrypted == false )
        continue;

     for( auto &mkey : mkeys )
     {
        try {
           //  encrypted keys are decrypted by using the masterkey and IV=sha256(sha256(pubkey))
           auto h = fc::sha256::hash( (char*)&key._pubkey[0], key._pubkey.size() );
           h = fc::sha256::hash( h );

           std::vector<unsigned char> iv( h.data(), h.data() + 16 );
           std::vector<unsigned char> privkeydata;
           if( decrypt( key._encprivkey, mkey, iv, privkeydata ) == false )
              continue;

           fc::sha256 hash( fc::to_hex( (char*)&privkeydata[0], 32 ) );
           fc::ecc::private_key privkey = fc::ecc::private_key::regenerate( hash );
           if( private_key_matches_public( privkey, key._pubkey ) )
           {
              key.set_private_key( privkey );
              break;
           }
        }
        catch ( const fc::exception& e )
        {
           wlog( "${e}", ("e",e.to_detail_string()) );
        }
     }
  }

  // collect all the keys
  for( auto &key : keys )
  {
     // skip still encrypted keys
     if( key._encrypted )
        continue;

     // add only named keys
   //  if( std::find( names.begin(), names.end(), key._address_compressed ) != names.end() ||
   //      std::find( names.begin(), names.end(), key._address_uncompressed ) != names.end() )
     {
        ekeys.push_back(key._privkey);
     }
  }
  //ilog( "keys: ${keys}", ("keys",ekeys) );

  return ekeys;
}

std::vector<fc::ecc::private_key> import_bitcoin_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{ try {

  wallet_db db( wallet_dat.to_native_ansi_path().c_str() );
  return collect_keys( db, passphrase );

} FC_RETHROW_EXCEPTIONS( warn, "" ) }

} } // bts::bitcoin
