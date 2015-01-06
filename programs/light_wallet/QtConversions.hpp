#pragma once

#include <QDateTime>
#include <QString>

#include <fc/time.hpp>
#include <fc/filesystem.hpp>

#include <string>

inline static fc::time_point_sec convert(const QDateTime& t)
{
   return fc::time_point_sec(t.toTime_t());
}
inline static QDateTime convert(const fc::time_point_sec t)
{
   return QDateTime::fromTime_t(t.sec_since_epoch());
}

inline static std::string convert(const QString& s)
{
   return s.toStdString();
}
inline static QString convert(const std::string& s)
{
   return QString::fromStdString(s);
}
