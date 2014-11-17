#pragma once

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariant>
#include <QJSValue>

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
   Q_INVOKABLE void get_verification_request_status(QString account_name, QStringList verifiers, QJSValue callback);

   std::shared_ptr<bts::client::client> get_client() { return _client; }

   void handle_crash();

   bool is_initialized() const {
      return _init_complete.ready();
   }

public Q_SLOTS:
   void set_data_dir(QString data_dir);
   void confirm_and_set_approval(QString delegate_name, bool approve);

   void begin_verification(QObject* window, QString account_name, QStringList verifiers, QJSValue callback);

Q_SIGNALS:
   void initialization_complete();
   void state_changed();
   void status_update(QString statusString);
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

   bts::blockchain::public_key_type lookup_public_key(QStringList verifiers);
   std::shared_ptr<bts::rpc::rpc_client> get_rpc_client(QString verifiers);
   Q_INVOKABLE void process_verifier_response(bts::mail::message response, QString account_name, QJSValue callback);
};
