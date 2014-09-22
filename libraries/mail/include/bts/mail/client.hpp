#pragma once

#include <bts/mail/message.hpp>
#include <bts/wallet/wallet.hpp>

#include <fc/network/ip.hpp>

#include <string>

namespace bts {
namespace mail {

namespace detail { class client_impl; struct mail_record; struct mail_archive_record; }

struct email_header;
struct email_record;

class client : public std::enable_shared_from_this<client> {
public:
    enum mail_status {
        submitted,              //The message has been submitted to the client for processing
        proof_of_work,          //The message is undergoing proof-of-work before transmission to server
        transmitting,           //The message is being transmitted to the servers, but has not yet been accepted by all of them
        accepted,               //The message has been acknowledged and stored by the server

        received,               //The message is an incoming message

        failed                  //The message could not be processed
    };

    client(wallet::wallet_ptr wallet, blockchain::chain_database_ptr chain);
    virtual ~client() {}

    void open(const fc::path& data_dir);

    void retry_message(message_id_type message_id);
    void remove_message(message_id_type message_id);
    void archive_message(message_id_type message_id_type);

    int check_new_messages();

    std::multimap<mail_status, message_id_type> get_processing_messages();
    std::multimap<mail_status, message_id_type> get_archive_messages();
    std::vector<email_header> get_inbox();
    email_record get_message(message_id_type message_id);

    message_id_type send_email(const string& from, const string& to, const string& subject, const string& body, const message_id_type& reply_to = message_id_type());

    std::vector<email_header> get_messages_by_sender(string sender);
    std::vector<email_header> get_messages_by_recipient(string recipient);
    std::vector<email_header> get_messages_from_to(string sender, string recipient);
private:
    std::shared_ptr<detail::client_impl> my;
};

struct email_header {
    fc::ripemd160 id;
    string sender;
    string recipient;
    string subject;
    fc::time_point_sec timestamp;

    email_header(){}
    email_header(const detail::mail_record& processing_record);
    email_header(const detail::mail_archive_record& archive_record);
};

struct email_record {
    email_header header;
    message content;
    fc::optional<std::unordered_set<fc::ip::endpoint>> mail_servers;
    fc::optional<string> failure_reason;

    email_record(){}
    email_record(const detail::mail_record& processing_record);
    email_record(const detail::mail_archive_record& archive_record);

    fc::ripemd160 id_on_server() {
        return content.id();
    }
};

}
}

FC_REFLECT_TYPENAME(bts::mail::client::mail_status)
FC_REFLECT_ENUM(bts::mail::client::mail_status, (submitted)(proof_of_work)(transmitting)(accepted)(received)(failed))
FC_REFLECT(bts::mail::email_header, (id)(sender)(recipient)(subject)(timestamp))
FC_REFLECT(bts::mail::email_record, (header)(content)(mail_servers)(failure_reason))
