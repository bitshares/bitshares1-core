#include <bts/mail/client.hpp>
#include <bts/db/level_map.hpp>

#include <fc/network/ip.hpp>

#ifndef UNUSED
#define UNUSED(var) ((void)(var))
#endif

namespace bts {
namespace mail {

using namespace fc;
using namespace bts::blockchain;
using namespace bts::wallet;
using std::string;

namespace detail {

class client_impl {
public:
    client* self;
    wallet_ptr _wallet;
    chain_database_ptr _chain;

    bts::db::level_map<client::mail_status, message_id_type> _mail_status_db;

    client_impl(client* self, wallet_ptr wallet, chain_database_ptr chain)
        : self(self),
          _wallet(wallet),
          _chain(chain)
    {}
    ~client_impl(){}

    uint160 proof_of_work_difficulty(ip::endpoint mail_server) {
        UNUSED(mail_server);
        //TODO: Something intelligent
        return ripemd160("000000ffffffffffffffffffffffffffffffffff");
    }

    void do_proof_of_work(message& email, ip::endpoint mail_server) {
        //TODO: Async this
        uint160 difficulty = proof_of_work_difficulty(mail_server);

        while (email.id() > difficulty)
            ++email.nonce;
    }

    ip::endpoint get_mail_server_for_recipient(const public_key_type& recipient) {
        UNUSED(recipient);
        //TODO: Check recipient's account on blockchain for his mail server
        return ip::endpoint(ip::address("127.0.0.1"), 1111);
    }

    message_id_type send_email(const string &from, const string &to, const string &subject, const string &body) {
        oaccount_record recipient = _chain->get_account_record(to);
        FC_ASSERT(recipient, "Could not find recipient account: ${name}", ("name", to));
        public_key_type recipient_key = recipient->active_key();

        message email = _wallet->mail_create(from, recipient_key, subject, body);

        do_proof_of_work(email, get_mail_server_for_recipient(recipient_key));
        //TODO: Compute proof of work, store email, send to server

        return email.id();
    }
};
}

client::client(wallet_ptr wallet, chain_database_ptr chain)
    : my(new detail::client_impl(this, wallet, chain))
{
}

message_id_type client::send_email(const string &from, const string &to, const string &subject, const string &body) {
    FC_ASSERT(my->_wallet->is_open());
    FC_ASSERT(my->_wallet->is_unlocked());

    return my->send_email(from, to, subject, body);
}

}
}
