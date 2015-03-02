#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSysInfo>
#include <QQmlContext>
#include <QQmlDebuggingEnabler>
#include <QtQml>

#include <bts/blockchain/time.hpp>

#include "QtWrappers.hpp"
#include "LightWallet.hpp"

int main(int argc, char *argv[])
{
   QGuiApplication app(argc, argv);
   app.setApplicationName(QStringLiteral("BitShares %1 Light Wallet").arg(BTS_BLOCKCHAIN_SYMBOL));
   app.setOrganizationName(BTS_BLOCKCHAIN_NAME);
   app.setOrganizationDomain("bitshares.org");
   app.setApplicationVersion("1.0 Beta 3");

   //Fire up the NTP system
   bts::blockchain::now();

#ifdef BTS_TEST_NETWORK
   QQmlDebuggingEnabler enabler;
#endif

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
#if QT_VERSION >= 0x050400
   engine.rootContext()->setContextProperty("PlatformName", QSysInfo::prettyProductName());
#endif
   engine.rootContext()->setContextProperty("ManifestUrl", QStringLiteral("https://bitshares.org/manifest.json"));
   engine.rootContext()->setContextProperty("AppName", QStringLiteral("lw_%1").arg(BTS_BLOCKCHAIN_SYMBOL).toLower());
   engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

   return app.exec();
}
