#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlDebuggingEnabler>
#include <QtQml>

#include <bts/blockchain/time.hpp>

#include "QtWrappers.hpp"
#include "LightWallet.hpp"

int main(int argc, char *argv[])
{
   QGuiApplication app(argc, argv);
   app.setApplicationName(QStringLiteral("BitShares Light Wallet"));
   app.setOrganizationName("BitShares");
   app.setOrganizationDomain("bitshares.org");

   //Fire up the NTP system
   bts::blockchain::now();

   QQmlDebuggingEnabler enabler;

   qmlRegisterType<LightWallet>("org.BitShares.Types", 1, 0, "LightWallet");
   qmlRegisterUncreatableType<Account>("org.BitShares.Types", 1, 0, "Account",
                                       QStringLiteral("Accounts can only be created in backend."));
   qmlRegisterUncreatableType<Balance>("org.BitShares.Types", 1, 0, "Balance",
                                       QStringLiteral("Balances can only be created in backend."));
   qmlRegisterUncreatableType<TransactionSummary>("org.BitShares.Types", 1, 0, "TransactionSummary",
                                                  QStringLiteral("Transaction summaries can only be created in backend."));
   qmlRegisterUncreatableType<LedgerEntry>("org.BitShares.Types", 1, 0, "LedgerEntry",
                                           QStringLiteral("Ledger entries can only be created in backend."));

   QQmlApplicationEngine engine;
   auto nam = engine.networkAccessManager();
   if( nam )
   {
      QNetworkDiskCache* cache = new QNetworkDiskCache(&engine);
      cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/cache");
      nam->setCache(cache);
   }
   engine.load(QUrl(QStringLiteral("qml/main.qml")));

   return app.exec();
}
