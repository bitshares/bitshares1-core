#pragma once

#include <QDateTime>
#include <QString>

#include <fc/time.hpp>
#include <fc/filesystem.hpp>

#include <string>

inline const static fc::time_point_sec convert(const QDateTime& t)
{
   return fc::time_point_sec(t.toTime_t());
}
inline const static QDateTime convert(const fc::time_point_sec& t)
{
   return QDateTime::fromTime_t(t.sec_since_epoch());
}

inline const static std::string convert(const QString& s)
{
   return s.toStdString();
}
inline const static QString convert(const std::string& s)
{
   return QString::fromStdString(s);
}
