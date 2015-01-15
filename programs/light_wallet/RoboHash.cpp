#include <RoboHash.hpp>

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QDebug>

QImage RoboHash::requestImage(const QString& id, QSize* size, const QSize& requestedSize)
{
   QUrl hashUrl(QStringLiteral("https://robohash.org/%1.png").arg(id));
   QNetworkAccessManager downloader;

   if( requestedSize.isValid() )
      hashUrl.setQuery(QStringLiteral("size=%1x%2").arg(requestedSize.width()).arg(requestedSize.height()));

   qDebug() << "Fetching" << hashUrl;

   QEventLoop loop;
   QObject::connect(&downloader, &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
   auto reply = downloader.get(QNetworkRequest(hashUrl));
   loop.exec();

   QImage hash = QImage::fromData(reply->readAll());
   if( size )
      *size = hash.size();
   return hash;
}
