#pragma once

#include <QQuickImageProvider>

class RoboHash : public QQuickImageProvider
{
public:
   RoboHash() : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading){}
   QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize);
};
