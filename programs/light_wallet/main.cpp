#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlDebuggingEnabler>

int main(int argc, char *argv[])
{
   QGuiApplication app(argc, argv);

   QQmlDebuggingEnabler enabler;

   QQmlApplicationEngine engine;
   engine.load(QUrl(QStringLiteral("qml/main.qml")));

   return app.exec();
}
