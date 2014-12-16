#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlDebuggingEnabler>
#include <QtQml>

#include "QtWrappers.hpp"
#include "LightWallet.hpp"

int main(int argc, char *argv[])
{
   QGuiApplication app(argc, argv);
   app.setApplicationName(QStringLiteral("BitShares Light Wallet"));
   app.setOrganizationName("BitShares");
   app.setOrganizationDomain("bitshares.org");

   QQmlDebuggingEnabler enabler;

   qmlRegisterType<LightWallet>("org.BitShares.Types", 1, 0, "LightWallet");

   QQmlApplicationEngine engine;
   engine.load(QUrl(QStringLiteral("qml/main.qml")));

   return app.exec();
}
