#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>
#include <bts/mail/message.hpp>

using namespace bts::wallet;
using namespace bts::wallet::detail;

bts::mail::message wallet::mail_create(const string& sender,
                                       const string& subject,
                                       const string& body,
                                       const bts::mail::message_id_type& reply_to)
{
    FC_ASSERT(is_open());
    FC_ASSERT(is_unlocked());

    mail::signed_email_message plaintext;
    plaintext.subject = subject;
    plaintext.reply_to = reply_to;
    plaintext.body = body;

    auto sender_key = get_active_private_key(sender);
    plaintext.sign(sender_key);

    return plaintext;
}

bts::mail::message wallet::mail_encrypt(const public_key_type& recipient, const bts::mail::message& plaintext)
{
    FC_ASSERT(is_open());
    FC_ASSERT(is_unlocked());

    auto one_time_key = my->_wallet_db.generate_new_one_time_key( my->_wallet_password );
    return plaintext.encrypt(one_time_key, recipient);
}

bts::mail::message wallet::mail_open(const address& recipient, const bts::mail::message& ciphertext)
{
    FC_ASSERT(is_open());
    FC_ASSERT(is_unlocked());
    if(!is_receive_address(recipient))
        //It's not to us... maybe it's from us.
        return mail_decrypt(recipient, ciphertext);

    auto recipient_key = get_active_private_key(my->_blockchain->get_account_record(recipient)->name);
    FC_ASSERT(ciphertext.type == mail::encrypted);
    return ciphertext.as<mail::encrypted_message>().decrypt(recipient_key);
}

bts::mail::message wallet::mail_decrypt(const address& recipient, const bts::mail::message& ciphertext)
{
    FC_ASSERT(is_open());
    FC_ASSERT(is_unlocked());
    FC_ASSERT(ciphertext.type == mail::encrypted, "Unknown message type");

    oaccount_record recipient_account = my->_blockchain->get_account_record(recipient);
    FC_ASSERT(recipient_account, "Unknown recipient address");
    public_key_type recipient_key = recipient_account->active_key();
    FC_ASSERT(recipient_key != public_key_type(), "Unknown recipient address");

    auto encrypted_message = ciphertext.as<mail::encrypted_message>();
    FC_ASSERT(my->_wallet_db.has_private_key(encrypted_message.onetimekey));
    owallet_key_record one_time_key = my->_wallet_db.lookup_key(encrypted_message.onetimekey);
    private_key_type one_time_private_key = one_time_key->decrypt_private_key(my->_wallet_password);

    auto secret = one_time_private_key.get_shared_secret(recipient_key);
    return encrypted_message.decrypt(secret);
}
