#include <bts/bitcoin/armory.hpp>
#include <bts/blockchain/pts_address.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/romix.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/fstream.hpp>

namespace bts { namespace bitcoin {

std::vector<fc::ecc::private_key> import_armory_wallet( const fc::path& wallet_dat, const std::string& passphrase )
{
    if( !fc::exists( wallet_dat ) )
        FC_THROW( ("Unable to open wallet: file \"" + wallet_dat.preferred_string() + "\" not found!").c_str() );

    std::vector<fc::ecc::private_key> output;

    fc::ifstream isWallet( wallet_dat, fc::ifstream::in | fc::ifstream::binary );

    #define skipB( n ) isWallet.seekg( n, fc::ifstream::cur )
    #define readLen( len ) { char buf[2]; isWallet.read( buf, 2 ); len = ((unsigned char)buf[1] << 8) | (unsigned char)buf[0]; }
    #define dumpB( n ) { char buf[n]; isWallet.read( buf, n ); for( int i = 0; i < n; i++ ) printf( "%02x", (unsigned char)buf[i] ); printf( "\n" ); }
    #define check( var, len ) \
    { \
        fc::sha256 hash = fc::sha256::hash(fc::sha256::hash( (char*)var, len )); \
          \
        char buf[4]; \
        isWallet.read(buf, 4); \
         \
        unsigned char nullHash[4] = { 0x5d, 0xf6, 0xe0, 0xe2 }; \
        char null[len]; \
        memset(null, 0, len); \
         \
        if( (memcmp( buf, nullHash, 4 ) || memcmp( (unsigned char*)var, null, len) ) && memcmp( &hash, buf, 4) ) \
        { \
            FC_LOG_MESSAGE( warn, "checksum invalid for "#var" ${nkeys} in wallet", ("nkeys", output.size() - 1) ); \
        } \
    }

    skipB(    8 ); // fileID
    skipB(    4 ); // version
    skipB(    4 ); // magic bytes
    skipB(    8 ); // wlt flags
    skipB(    6 ); // binUniqueID
    skipB(    8 ); // create date
    skipB(   32 ); // short name
    skipB(  256 ); // long name
    skipB(    8 ); // highest used

    // read Crypto/KDF data
    unsigned char kdfData[44];
    isWallet.read( (char*)kdfData, 44 );
    check( kdfData, 44 );

    uint64_t mem = 0;
    mem += (uint64_t)kdfData[0] << (0 * 8);
    mem += (uint64_t)kdfData[1] << (1 * 8);
    mem += (uint64_t)kdfData[2] << (2 * 8);
    mem += (uint64_t)kdfData[3] << (3 * 8);
    mem += (uint64_t)kdfData[4] << (4 * 8);
    mem += (uint64_t)kdfData[5] << (5 * 8);
    mem += (uint64_t)kdfData[6] << (6 * 8);
    mem += (uint64_t)kdfData[7] << (7 * 8);

    uint32_t nIter = 0;
    nIter += (uint32_t)kdfData[ 8] << (0 * 8);
    nIter += (uint32_t)kdfData[ 9] << (1 * 8);
    nIter += (uint32_t)kdfData[10] << (2 * 8);
    nIter += (uint32_t)kdfData[11] << (3 * 8);

    std::string salt( (char*)&kdfData[12], 32 );

    fc::romix kdfRomix(mem, nIter, salt);
    std::string masterKey = kdfRomix.deriveKey( passphrase );

    skipB( 512 - 44 - 4 ); // skip remaining bytes

    // KeyGen / sizeof(address), varies with compressed keys:
    skipB( 148 );

    char keyType;
    isWallet.read(&keyType, 1);

    skipB( keyType == 4 ? 88 : 56 );

    skipB( 1024 ); // unused

    enum EntryType
    {
        WLT_DATATYPE_KEYDATA = 0,
        WLT_DATATYPE_ADDRCOMMENT = 1,
        WLT_DATATYPE_TXCOMMENT = 2,
        WLT_DATATYPE_OPEVAL = 3,
        WLT_DATATYPE_DELETED = 4
    };

    try
    {
        while( !isWallet.eof() )
        {
            // read entry header
            unsigned char type;
            isWallet.read( (char*)&type, 1 );

            uint16_t len = 0;
            switch( type )
            {
            case WLT_DATATYPE_KEYDATA:
            {
                skipB( 20 ); // hash 20 bytes

                skipB( 20 ); // addr hash
                skipB(  4 ); // addr check
                skipB(  4 ); // addr version
                skipB(  8 ); // flags
                skipB( 32 ); // chain code
                skipB(  4 ); // chain check
                skipB(  8 ); // chain index
                skipB(  8 ); // chain depth

                char iv[16];
                char buf[4];

                // read IV
                isWallet.read( iv, 16 );
                check( iv, 16 );

                // read private key
                fc::sha256 privateKey;
                isWallet.read( (char*)&privateKey, 32 );
                check( &privateKey, 32 );

                if( std::string( 16, 0 ) != std::string( iv, 16 ) )
                {
                    fc::sha256 decryptedKey;

                    try
                    {
                        fc::aes_cfb_decrypt( (unsigned char*)&privateKey, 32, (unsigned char*)masterKey.c_str(), (unsigned char*)iv, (unsigned char*)&decryptedKey );
                    }
                    catch (fc::exception &)
                    {
                        FC_LOG_MESSAGE( warn, "Decryption failed for key ${nkey}! ", ( "nkey", output.size() ));
                    }

                    privateKey = decryptedKey;
                }

                fc::ecc::private_key key = fc::ecc::private_key::regenerate( privateKey );
                fc::ecc::public_key pkey = key.get_public_key();
                bool keyMatch = false;

                isWallet.read( buf, 1 );
                if (buf[0] == 4)
                {
                    // uncompressed pubkey (bitcoin?)
                    fc::ecc::public_key_point_data publicKey;
                    publicKey.at(0) = buf[0];

                    isWallet.read( &publicKey.at(1), 64 );
                    check( &publicKey.at(0), 65 );

                    if (pkey.serialize_ecc_point() == publicKey)
                        keyMatch = true;
                }

                else
                {
                    // compressed pubkey
                    fc::ecc::public_key_data publicKey;
                    publicKey.at(0) = buf[0];

                    isWallet.read( &publicKey.at(1), 32 );
                    check( &publicKey.at(0), 33 );

                    if (pkey.serialize() == publicKey)
                        keyMatch = true;
                }


                if (keyMatch)
                    output.push_back( key );

                skipB(  8 ); // first time
                skipB(  8 ); // last time
                skipB(  4 ); // first block
                skipB(  4 ); // last block

                break;
            }

            case WLT_DATATYPE_ADDRCOMMENT:
                skipB( 20 ); // hash 20 bytes
                readLen( len );
                skipB( len ); // txt
                break;

            case WLT_DATATYPE_TXCOMMENT:
                skipB( 20 ); // hash 32 bytes
                readLen( len );
                skipB( len ); // txt
                break;

            default:
            case WLT_DATATYPE_OPEVAL:
                // not implemented
                break;

            case WLT_DATATYPE_DELETED:
                readLen( len );
                skipB( len );
                break;
            }
        }
    }
    catch( fc::eof_exception& )
    {
        // ifstream.eof seems to not work as intended, triggering this every time, so do nothing here
    }

    return output;
}

} } // bts::bitcoin
