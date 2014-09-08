#include <bts/mail/client.hpp>
#include <bts/db/level_map.hpp>
#include <bts/db/cached_level_map.hpp>

#include <fc/network/ip.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/io/buffered_iostream.hpp>

#include <queue>

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

struct mail_record {
    mail_record(string sender = string(),
                string recipient = string(),
                public_key_type recipient_key = public_key_type(),
                message&& content = message())
        : id(content.id()),
          status(client::submitted),
          sender(sender),
          recipient(recipient),
          recipient_key(recipient_key),
          content(content)
    {}

    //Static email ID; not to be confused with proof-of-work hash in content.id()
    ripemd160 id;
    client::mail_status status;
    string sender;
    string recipient;
    public_key_type recipient_key;
    message content;
    std::unordered_set<ip::endpoint> mail_servers;
    ripemd160 proof_of_work_target;
    string failure_reason;
};

class client_impl {
public:
    client* self;
    wallet_ptr _wallet;
    chain_database_ptr _chain;

    typedef std::queue<message_id_type> job_queue;
    job_queue _proof_of_work_jobs;
    fc::future<void> _proof_of_work_worker;
    job_queue _transmit_message_jobs;
    fc::future<void> _transmit_message_worker;

    bts::db::cached_level_map<message_id_type, mail_record> _processing_db;
    bts::db::level_map<message_id_type, mail_record> _archive;

    client_impl(client* self, wallet_ptr wallet, chain_database_ptr chain)
        : self(self),
          _wallet(wallet),
          _chain(chain)
    {}
    ~client_impl(){
        _proof_of_work_worker.cancel_and_wait("Mail client destroyed");

        _archive.close();
        _processing_db.close();
    }

    void retry_message(mail_record email) {
        switch (email.status) {
        case client::submitted:
            process_outgoing_mail(email);
            break;
        case client::proof_of_work:
            schedule_proof_of_work(email.id);
            break;
        case client::transmitting:
            schedule_transmit_message(email.id);
            break;
        case client::accepted:
            finalize_message(email.id);
            break;
        case client::failed:
            //Do nothing
            break;
        }
    }

    void open(const fc::path& data_dir) {
        _archive.open(data_dir / "archive");
        _processing_db.open(data_dir / "processing");

        //Place all in-processing messages back in their place on the pipeline
        for (auto itr = _processing_db.begin(); itr.valid(); ++itr)
            retry_message(itr.value());
    }
    bool is_open() {
        return _archive.is_open();
    }

    void process_outgoing_mail(mail_record& mail) {
        //Messages go through a pipeline of processing. This function starts them on that journey.
        set_mail_servers_on_record(mail);
        _processing_db.store(mail.id, mail);

        //The steps required to send a message:
        //Get proof of work target from mail servers
        //Calculate proof of work
        //Send message to all applicable mail servers
        //Store message in the archive

        get_proof_of_work_target(mail.id);
    }

    void set_mail_servers_on_record(mail_record& record) {
        //TODO: Check recipient's account on blockchain for his mail server
        record.mail_servers.insert(ip::endpoint(ip::address("127.0.0.1"), 3000));
    }

    void get_proof_of_work_target(const message_id_type& message_id) {
        mail_record email = _processing_db.fetch(message_id);

        if (email.mail_servers.empty()) {
            email.status = client::failed;
            email.failure_reason = "Could not find mail servers for this recipient.";
            _processing_db.store(message_id, email);
            return;
        }

        //TODO: Contact mail servers, get their PoW requirements, set target to min() of these
        email.proof_of_work_target = ripemd160("000000fffdeadbeeffffffffffffffffffffffff");
        _processing_db.store(message_id, email);

        schedule_proof_of_work(message_id);
    }

    template<typename TaskData>
    void schedule_generic_task(job_queue& queue,
                               fc::future<void>& future,
                               TaskData data,
                               std::function<void(TaskData)> task,
                               const char* task_description) {
        queue.push(data);

        if (future.valid() && !future.ready())
            return;

        future = fc::async([task, task_description, &queue]{
            while (!queue.empty()) {
                TaskData data = queue.front();
                queue.pop();
                task(data);
            }
        }, task_description);
    }

    void schedule_proof_of_work(const message_id_type& message_id) {
        schedule_generic_task<message_id_type>(_proof_of_work_jobs, _proof_of_work_worker, message_id, [this](message_id_type message_id){
            mail_record email = _processing_db.fetch(message_id);

            if (email.proof_of_work_target != ripemd160()) {
                email.status = client::proof_of_work;
                _processing_db.store(email.id, email);
            } else {
                //Don't have a proof-of-work target; cannot continue
                email.status = client::failed;
                email.failure_reason = "No proof of work target. Cannot do proof of work.";
                _processing_db.store(email.id, email);
                return;
            }

            uint8_t yielder = 0;
            while (email.content.id() > email.proof_of_work_target) {
                if(++yielder == 0)
                    fc::yield();
                ++email.content.nonce;
            }

            _processing_db.store(email.id, email);
            schedule_transmit_message(email.id);
            fc::yield();
        }, "Mail client proof-of-work");
    }

    void schedule_transmit_message(message_id_type message_id) {
        schedule_generic_task<message_id_type>(_transmit_message_jobs, _transmit_message_worker, message_id, [this](message_id_type message_id){
            mail_record email = _processing_db.fetch(message_id);
            if (email.mail_servers.empty()) {
                email.status = client::failed;
                email.failure_reason = "No mail servers found when trying to transmit message.";
                _processing_db.store(message_id, email);
                return;
            } else {
                email.status = client::transmitting;
                _processing_db.store(message_id, email);
            }

            vector<fc::future<void>> transmit_tasks;
            transmit_tasks.reserve(email.mail_servers.size());
            for (ip::endpoint server : email.mail_servers) {
                transmit_tasks.push_back(fc::async([=] {
                    auto email = _processing_db.fetch(message_id);
                    tcp_socket sock;
                    sock.connect_to(server);

                    mutable_variant_object request;
                    request["id"] = 0;
                    request["method"] = "mail_store_message";
                    request["params"] = vector<variant>({variant(address(email.recipient_key)), variant(email.content)});

                    fc::json::to_stream(sock, variant_object(request));
                    string raw_response;
                    fc::getline(sock, raw_response);
                    variant_object response = fc::json::from_string(raw_response).as<variant_object>();

                    if (response["id"].as_int64() != 0)
                        wlog("Server response has wrong ID... attempting to press on. Expected: 0; got: ${r}", ("r", response["id"]));
                    if (response.contains("error")) {
                        email.status = client::failed;
                        email.failure_reason = response["error"].as<variant_object>()["data"].as<variant_object>()["message"].as_string();
                        _processing_db.store(message_id, email);
                        elog("Storing message with server ${server} failed: ${error}", ("server", server)("error", response["error"])("request", request));
                        sock.close();
                        return;
                    }

                    request["id"] = 1;
                    request["method"] = "mail_fetch_message";
                    request["params"] = vector<variant>({variant(email.content.id())});

                    fc::json::to_stream(sock, variant_object(request));
                    fc::getline(sock, raw_response);
                    response = fc::json::from_string(raw_response).as<variant_object>();

                    if (response["id"].as_int64() != 1)
                        wlog("Server response has wrong ID... attempting to press on. Expected: 1; got: ${r}", ("r", response["id"]));
                    if (response["result"].as<message>().id() != email.content.id()) {
                        email.status = client::failed;
                        email.failure_reason = "Message saved to server, but server responded with another message when we requested it.";
                        _processing_db.store(message_id, email);
                        elog("Storing message with server ${server} failed because server gave back wrong message.", ("server", server));
                        sock.close();
                        return;
                    }
                }, "Mail client transmitter"));
            }

            auto timeout_future = fc::schedule([=] {
                auto email = _processing_db.fetch(message_id);
                ulog("Email ${id}: Timeout when transmitting", ("id", email.id));
                email.status = client::failed;
                email.failure_reason = "Timed out while transmitting message.";
                _processing_db.store(email.id, email);
                for (auto task_future : transmit_tasks)
                    task_future.cancel();
            }, fc::time_point::now() + fc::seconds(10), "Mail client transmitter timeout");

            while (!transmit_tasks.empty()) {
                transmit_tasks.back().wait();
                if (email.status == client::failed) {
                    for (auto task_future : transmit_tasks)
                        task_future.cancel_and_wait();
                    return;
                }
                transmit_tasks.pop_back();
            }
            timeout_future.cancel("Finished transmitting");

            _processing_db.store(message_id, email);
            if (email.status != client::failed)
                finalize_message(message_id);
        }, "Mail client transmit message");
    }

    void finalize_message(message_id_type message_id) {
        ulog("Email ${id} sent successfully.", ("id", message_id));
        mail_record email = _processing_db.fetch(message_id);
        email.status = client::accepted;
        //TODO: Change archive to use a different struct with more appropriate contents
        _archive.store(message_id, email);
        _processing_db.remove(message_id);
    }

    std::multimap<client::mail_status, message_id_type> get_processing_messages() {
        std::multimap<client::mail_status, message_id_type> messages;
        for(auto itr = _processing_db.begin(); itr.valid(); ++itr) {
            mail_record email = itr.value();
            messages.insert(std::make_pair(email.status, email.id));
        }
        return messages;
    }

    message get_message(message_id_type message_id) {
        if (_processing_db.fetch_optional(message_id))
            return _processing_db.fetch_optional(message_id)->content;
        if (_archive.fetch_optional(message_id))
            return _archive.fetch_optional(message_id)->content;
        FC_ASSERT(false, "Message ${id} not found.", ("id", message_id));
    }
};

}

client::client(wallet_ptr wallet, chain_database_ptr chain)
    : my(new detail::client_impl(this, wallet, chain))
{
}

void client::open(const path& data_dir) {
    my->open(data_dir);
}

void client::retry_message(message_id_type message_id)
{
    FC_ASSERT(my->is_open());
    auto itr = my->_processing_db.find(message_id);
    FC_ASSERT(itr.valid(), "Message not found.");
    auto email = itr.value();
    FC_ASSERT(email.status == failed, "Message has not failed to send; cannot retry sending.");
    email.status = submitted;
    my->retry_message(email);
}

void client::remove_message(message_id_type message_id)
{
    FC_ASSERT(my->is_open());
    auto itr = my->_processing_db.find(message_id);
    if (itr.valid()) {
        FC_ASSERT(itr.value().status == failed, "Cannot remove message during processing.");
        my->_processing_db.remove(message_id);
    } else {
        auto itr = my->_archive.find(message_id);
        if (itr.valid())
            my->_archive.remove(message_id);
    }
}

std::multimap<client::mail_status, message_id_type> client::get_processing_messages() {
    FC_ASSERT(my->is_open());
    return my->get_processing_messages();
}

message client::get_message(message_id_type message_id) {
    FC_ASSERT(my->is_open());
    return my->get_message(message_id);
}

message_id_type client::send_email(const string &from, const string &to, const string &subject, const string &body) {
    FC_ASSERT(my->_wallet->is_open());
    FC_ASSERT(my->_wallet->is_unlocked());
    FC_ASSERT(my->is_open());

    //TODO: Find a thin-clienty way to do this, rather than calling a local chain_database
    oaccount_record recipient = my->_chain->get_account_record(to);
    FC_ASSERT(recipient, "Could not find recipient account: ${name}", ("name", to));
    public_key_type recipient_key = recipient->active_key();

    detail::mail_record email(from,
                              to,
                              recipient_key,
                              my->_wallet->mail_create(from, recipient_key, subject, body));
    my->process_outgoing_mail(email);

    return email.id;
}

}
}

FC_REFLECT(bts::mail::detail::mail_record, (id)(status)(sender)(recipient)(recipient_key)(content)(mail_servers)(proof_of_work_target))
