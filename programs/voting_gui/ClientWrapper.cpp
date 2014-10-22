#include "ClientWrapper.hpp"

#include <bts/blockchain/time.hpp>
#include <bts/net/upnp.hpp>
#include <bts/net/config.hpp>
#include <bts/db/exception.hpp>

#include <QGuiApplication>
#include <QResource>
#include <QSettings>
#include <QJsonDocument>
#include <QUrl>
#include <QMessageBox>
#include <QDir>

#include <iostream>

void ClientWrapper::get_htdocs_file( const fc::path& filename, const fc::http::server::response& r )
{
  std::cout << filename.generic_string() << "\n";

  auto give_404 = [&] {
    elog("404 on file ${name}", ("name", filename));
    std::string not_found = "this is not the file you are looking for: " + filename.generic_string();
    r.set_status(fc::http::reply::NotFound);
    r.set_length(not_found.size());
    r.write(not_found.c_str(), not_found.size());
  };

  auto give_200 = [&] (const char* data, uint64_t size) {
    r.set_status(fc::http::reply::OK);
    r.set_length(size);
    r.write(data, size);
  };

  //Check the update package first, then fall back to QRC.
  if (!_web_package.empty()) {
    auto file = _web_package.find(filename.to_native_ansi_path());
    if (file == _web_package.end())
      return give_404();
    return give_200(file->second.data(), file->second.size());
  }

  //No update package. Use QRC.
  QResource  file_to_send(("/htdocs/htdocs/" + filename.generic_string()).c_str());
  if(!file_to_send.data())
    return give_404();

  r.set_status(fc::http::reply::OK);
  if(file_to_send.isCompressed())
  {
    auto data = qUncompress(file_to_send.data(), file_to_send.size());
    return give_200(data.data(), data.size());
  }
  return give_200((const char*)file_to_send.data(), file_to_send.size());
}

ClientWrapper::ClientWrapper(QObject *parent)
  : QObject(parent),
    _main_thread(&fc::thread::current()),
    _bitshares_thread("bitshares"),
    _settings("BitShares", BTS_BLOCKCHAIN_NAME)
{
    _block_interval_timer.setInterval(1000);
    connect(&_block_interval_timer, &QTimer::timeout, this, &ClientWrapper::state_changed);
    _block_interval_timer.start();
}

ClientWrapper::~ClientWrapper()
{
  _init_complete.wait();
  QSettings("BitShares", BTS_BLOCKCHAIN_NAME).setValue("crash_state", "no_crash");
  if (_client)
     _bitshares_thread.async([this]{
       _client->stop();
       _client_done.wait();
       _client.reset();
     }).wait();
}

void ClientWrapper::handle_crash()
{
  auto response = QMessageBox::question(nullptr,
                                        tr("Crash Detected"),
                                        tr("It appears that %1 crashed last time it was running. "
                                           "If this is happening frequently, it could be caused by a "
                                           "corrupted database. Would you like "
                                           "to reset the database (this will take several minutes) or "
                                           "to continue normally? Resetting the database will "
                                           "NOT lose any of your information or funds.").arg(qApp->applicationName()),
                                        tr("Reset Database"),
                                        tr("Continue Normally"),
                                        QString(), 1);
  if (response == 0)
    QDir((get_data_dir() + "/chain")).removeRecursively();
}

void ClientWrapper::set_web_package(std::unordered_map<std::string, std::vector<char>>&& web_package)
{
  wlog("Using update package to serve web GUI");
  _web_package = std::move(web_package);
}

QString ClientWrapper::get_data_dir()
{
  QString data_dir = QString::fromStdWString(bts::client::get_data_dir(boost::program_options::variables_map()).generic_wstring());
  if (_settings.contains("data_dir"))
      data_dir = _settings.value("data_dir").toString();
  int data_dir_index = qApp->arguments().indexOf("--data-dir");
  if (data_dir_index != -1 && qApp->arguments().size() > data_dir_index+1)
      data_dir = qApp->arguments()[data_dir_index+1];

  return data_dir;
}

void ClientWrapper::initialize()
{
  bool upnp = _settings.value( "network/p2p/use_upnp", true ).toBool();

  std::string default_wallet_name = _settings.value("client/default_wallet_name", "default").toString().toStdString();
  _settings.setValue("client/default_wallet_name", QString::fromStdString(default_wallet_name));

#ifdef _WIN32
  _cfg.rpc.rpc_user = "";
  _cfg.rpc.rpc_password = "";
#else
  _cfg.rpc.rpc_user     = "randomuser";
  _cfg.rpc.rpc_password = fc::variant(fc::ecc::private_key::generate()).as_string();
#endif
  _cfg.rpc.httpd_endpoint = fc::ip::endpoint::from_string( "127.0.0.1:9999" );
  _cfg.rpc.httpd_endpoint.set_port(0);
  ilog( "config: ${d}", ("d", fc::json::to_pretty_string(_cfg) ) );

  auto data_dir = get_data_dir();
  wlog("Starting client with data-dir: ${ddir}", ("ddir", fc::path(data_dir.toStdWString())));

  _init_complete = _bitshares_thread.async( [=](){
    try
    {
      _main_thread->async( [&]{ Q_EMIT status_update(tr("Starting %1").arg(qApp->applicationName())); });
      _client = std::make_shared<bts::client::client>("qt_wallet");
      _client->open( data_dir.toStdWString(), fc::optional<fc::path>(), [=](float progress) {
         _main_thread->async( [=]{ Q_EMIT status_update(tr("Reindexing database... Approximately %1% complete.").arg(progress, 0, 'f', 0)); } );
      } );

      if(!_client->get_wallet()->is_enabled())
          _main_thread->async([&]{ Q_EMIT error(tr("Wallet is disabled in your configuration file. Please enable the wallet and relaunch the application.")); });

      // setup  RPC / HTTP services
      _main_thread->async( [&]{ Q_EMIT status_update(tr("Loading...")); });
      _client->get_rpc_server()->set_http_file_callback([this](const fc::path& filename, const fc::http::server::response& r) {
          get_htdocs_file(filename, r);
      });
      _client->get_rpc_server()->configure_http( _cfg.rpc );
      _actual_httpd_endpoint = _client->get_rpc_server()->get_httpd_endpoint();

      // load config for p2p node.. creates cli
      const bts::client::config& loadedCfg = _client->configure( data_dir.toStdWString() );

      _client->init_cli();

      _main_thread->async( [&]{ Q_EMIT status_update(tr("Connecting to %1 network").arg(qApp->applicationName())); });
      _client->listen_on_port(0, false /*don't wait if not available*/);
      fc::ip::endpoint actual_p2p_endpoint = _client->get_p2p_listening_endpoint();

      _client->set_daemon_mode(true);
      _client_done = _client->start();
      if( !_actual_httpd_endpoint )
      {
        _main_thread->async( [&]{ Q_EMIT error( tr("Unable to start HTTP server...")); });
      }

      _client->start_networking([=]{
        for (std::string default_peer : _cfg.default_peers)
          _client->connect_to_peer(default_peer);
      });

      if( upnp )
      {
        _upnp_service = std::make_shared<bts::net::upnp_service>();
        _upnp_service->map_port( actual_p2p_endpoint.port() );
      }

      try
      {
        _client->wallet_open(default_wallet_name);
      }
      catch(...)
      {}

      _main_thread->async( [&]{ Q_EMIT initialized(); });
    }
    catch (const bts::db::db_in_use_exception&)
    {
      _main_thread->async( [&]{ Q_EMIT error( tr("An instance of %1 is already running! Please close it and try again.").arg(qApp->applicationName())); });
    }
    catch (...)
    {
      ilog("Failure when attempting to initialize client");
      if (fc::exists(fc::path(data_dir.toStdWString()) / "chain")) {
        fc::remove_all(fc::path(data_dir.toStdWString()) / "chain");
        _main_thread->async( [&]{ Q_EMIT error( tr("An error occurred while trying to start. Please try restarting the application.")); });
      } else
        _main_thread->async( [&]{ Q_EMIT error( tr("An error occurred while trying to start.")); });
    }
  });
}

QUrl ClientWrapper::http_url() const
{
  QUrl url = QString::fromStdString("http://" + std::string( *_actual_httpd_endpoint ) );
  url.setUserName(_cfg.rpc.rpc_user.c_str() );
  url.setPassword(_cfg.rpc.rpc_password.c_str() );
  return url;
}

QString ClientWrapper::get_info(  )
{
  fc::variant_object result = _bitshares_thread.async( [this](){ return _client->get_info(); }).wait();
  return QString::fromStdString(fc::json::to_pretty_string( result ));
}

QString ClientWrapper::get_http_auth_token()
{
  QByteArray result = _cfg.rpc.rpc_user.c_str();
  result += ":";
  result += _cfg.rpc.rpc_password.c_str();
  return result.toBase64( QByteArray::Base64Encoding | QByteArray::KeepTrailingEquals );
}

void ClientWrapper::set_data_dir(QString data_dir)
{
  QSettings ("BitShares", BTS_BLOCKCHAIN_NAME).setValue("data_dir", data_dir);
}

void ClientWrapper::confirm_and_set_approval(QString delegate_name, bool approve)
{
  auto account = get_client()->blockchain_get_account(delegate_name.toStdString());
  if( account.valid() && account->is_delegate() )
  {
    if( QMessageBox::question(nullptr,
                              tr("Set Delegate Approval"),
                              tr("Would you like to update approval rating of Delegate %1 to %2?")
                              .arg(delegate_name)
                              .arg(approve?"Approve":"Disapprove")
                              )
        == QMessageBox::Yes )
      get_client()->wallet_account_set_approval(delegate_name.toStdString(), approve);
  }
  else
    QMessageBox::warning(nullptr, tr("Invalid Account"), tr("Account %1 is not a delegate, so its approval cannot be set.").arg(delegate_name));
}
