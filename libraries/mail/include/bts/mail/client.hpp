#pragma once

#include <bts/mail/message.hpp>
#include <bts/wallet/wallet.hpp>

#include <string>

namespace bts {
namespace mail {

namespace detail { class client_impl; }

class client : public std::enable_shared_from_this<client> {
public:
    enum mail_status {
        submitted,              //The message has been submitted to the client for processing
        proof_of_work,          //The message is undergoing proof-of-work before transmission to server
        transmitted,            //The message has been transmitted to the server, but not yet accepted
        accepted,               //The message has been acknowledged and stored by the server

        failed                  //The message could not be processed
    };

    client(wallet::wallet_ptr wallet, blockchain::chain_database_ptr chain);
    virtual ~client() {}

    void open(const fc::path& data_dir);

    message_id_type send_email(const string& from, const string& to, const string& subject, const string& body);

private:
    std::shared_ptr<detail::client_impl> my;
};

}
}

FC_REFLECT_TYPENAME(bts::mail::client::mail_status)
FC_REFLECT_ENUM(bts::mail::client::mail_status, (submitted)(proof_of_work)(transmitted)(accepted)(failed))
