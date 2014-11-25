#pragma once

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariant>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>

#include <bts/rpc/rpc_server.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/client/client.hpp>
#include <bts/net/upnp.hpp>

class ClientWrapper : public QObject
{
   Q_OBJECT
   Q_PROPERTY(bool initialized READ is_initialized NOTIFY initialization_complete)
   Q_PROPERTY(QString info READ get_info NOTIFY state_changed)

public:
   ClientWrapper(QObject *parent = nullptr);
   virtual ~ClientWrapper();

   ///Not done in constructor to allow caller to connect to error()
   void initialize();
   void wait_for_initialized() {
      _init_complete.wait();
   }

   QString get_data_dir();
   Q_INVOKABLE QString get_info();

   Q_INVOKABLE QString create_voter_account();
   Q_INVOKABLE QJsonObject get_voter_ballot(QString account_name);
   Q_INVOKABLE QJsonArray get_voter_contests(QString account_name);

   std::shared_ptr<bts::client::client> get_client() { return _client; }

   void handle_crash();

   bool is_initialized() const {
      return _init_complete.ready();
   }

public Q_SLOTS:
   void set_data_dir(QString data_dir);
   void load_election();

   void begin_verification(QObject* window, QString account_name, QStringList verifiers, QJSValue callback);
   void get_verification_request_status(QString account_name, QStringList verifiers, QJSValue callback);
   void process_accepted_identity(QString account_name, QString identity, QJSValue callback);
   void begin_registration(QString account_name, QStringList registrars);

Q_SIGNALS:
   void initialization_complete();
   void state_changed();
   void status_update(QString statusString);
   void registered();
   void error(QString errorString);

private:
   bts::client::config                  _cfg;
   std::shared_ptr<bts::client::client> _client;
   fc::future<void>                     _client_done;
   fc::thread*                          _main_thread;
   fc::thread                           _bitshares_thread;
   fc::future<void>                     _init_complete;
   fc::optional<fc::ip::endpoint>       _actual_httpd_endpoint;
   QSettings                            _settings;
   QTimer                               _block_interval_timer;

   std::shared_ptr<bts::net::upnp_service> _upnp_service;

   std::unordered_map<bts::vote::digest_type, bts::vote::ballot> _ballot_map;
   std::unordered_map<bts::vote::digest_type, bts::vote::contest> _contest_map;

   bts::blockchain::public_key_type lookup_public_key(QString account_name);
   std::shared_ptr<bts::rpc::rpc_client> get_rpc_client(QString account);
   Q_INVOKABLE void process_verifier_response(bts::mail::message response, QString account_name, QJSValue callback);
   Q_INVOKABLE void notify_acceptances(int acceptance_count, QJSValue callback);
   std::vector<bts::vote::signed_identity_property> merge_identities(const bts::vote::signed_identity& old_id,
                                                                     bts::vote::identity new_id);
   void purge_invalid_signatures(std::vector<bts::vote::signed_identity_property>& properties,
                                 bts::blockchain::address owner);
};
