#include <bts/blockchain/chain_interface.hpp>
#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet_records.hpp>
#include <fc/crypto/aes.hpp>

namespace bts { namespace wallet {

    extended_private_key master_key::decrypt_key( const fc::sha512& password )const
    { try {
       FC_ASSERT( password != fc::sha512() );
       FC_ASSERT( encrypted_key.size() );
       auto plain_text = fc::aes_decrypt( password, encrypted_key );
       return fc::raw::unpack<extended_private_key>( plain_text );
    } FC_CAPTURE_AND_RETHROW() }

    bool master_key::validate_password( const fc::sha512& password )const
    {
       return checksum == fc::sha512::hash( password );
    }

    void master_key::encrypt_key( const fc::sha512& password,
                                  const extended_private_key& k )
    { try {
       FC_ASSERT( password != fc::sha512() );
       checksum = fc::sha512::hash( password );
       encrypted_key = fc::aes_encrypt( password, fc::raw::pack( k ) );

       // just to double check... we should find out if there is
       // a problem ASAP...
       extended_private_key k_dec = decrypt_key( password );
       FC_ASSERT( k_dec == k );
    } FC_CAPTURE_AND_RETHROW() }

    bool key_data::has_private_key()const
    {
       return encrypted_private_key.size() > 0;
    }

    void key_data::encrypt_private_key( const fc::sha512& password,
                                        const private_key_type& key_to_encrypt )
    { try {
       FC_ASSERT( password != fc::sha512() );
       public_key            = key_to_encrypt.get_public_key();
       encrypted_private_key = fc::aes_encrypt( password, fc::raw::pack( key_to_encrypt ) );

       // just to double check, we should find out if there is a problem ASAP
       FC_ASSERT( key_to_encrypt == decrypt_private_key( password ) );
    } FC_CAPTURE_AND_RETHROW() }

    private_key_type key_data::decrypt_private_key( const fc::sha512& password )const
    { try {
       FC_ASSERT( password != fc::sha512() );
       const auto plain_text = fc::aes_decrypt( password, encrypted_private_key );
       return fc::raw::unpack<private_key_type>( plain_text );
    } FC_CAPTURE_AND_RETHROW() }

    contact_data::contact_data( const chain_interface& db, const string& data, const string& label )
    { try {
        contact_data record;

        if( db.get_account_record( data ).valid() )
        {
            record = contact_data( data );
        }
        else
        {
            try
            {
                record = contact_data( public_key_type( data ) );
            }
            catch( const fc::exception& )
            {
                try
                {
                    record = contact_data( address( data ) );
                }
                catch( const fc::exception& )
                {
                    try
                    {
                        record = contact_data( pts_address( data ) );
                    }
                    catch( const fc::exception& )
                    {
                        FC_CAPTURE_AND_THROW( invalid_contact, (data) );
                    }
                }
            }
        }

        if( !label.empty() )
            record.label = label;

        *this = std::move( record );
    } FC_CAPTURE_AND_RETHROW( (data)(label) ) }

    approval_data::approval_data( const chain_interface& db, const string& name, const int8_t approval )
    { try {
        FC_ASSERT( db.get_account_record( name ).valid() );

        approval_data record;
        record.name = name;
        record.approval = approval;

        *this = std::move( record );
    } FC_CAPTURE_AND_RETHROW( (name)(approval) ) }

} } // bts::wallet
