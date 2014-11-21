#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlDebuggingEnabler>

#include "ClientWrapper.hpp"
#include "Utilities.hpp"

Q_DECLARE_METATYPE(bts::mail::message)
int main(int argc, char *argv[])
{
   QGuiApplication app(argc, argv);

   ClientWrapper* client = new ClientWrapper(&app);
   client->initialize();

   qRegisterMetaType<bts::mail::message>();

   QQmlDebuggingEnabler enabler;

   QQmlApplicationEngine engine;
   QQmlContext* context = engine.rootContext();
   context->setContextProperty("bitshares", client);
   context->setContextProperty("utilities", new Utilities(client));
   engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

   return app.exec();
}
