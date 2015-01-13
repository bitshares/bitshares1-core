#pragma once

#include <QDateTime>
#include <QLatin1String>
#include <QObject>
#include <QDebug>

#include <bts/light_wallet/light_wallet.hpp>

#include "QtConversions.hpp"

#define GETTER_(TYPE, INTERNAL_NAME, EXTERNAL_NAME) \
   TYPE EXTERNAL_NAME() { \
      return convert(m.INTERNAL_NAME); \
   }
#define GETTER(TYPE, NAME) GETTER_(TYPE, NAME, NAME)

class LightTransactionSummary : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QDateTime when READ when CONSTANT)
   Q_PROPERTY(QString from READ from CONSTANT)
   Q_PROPERTY(QString to READ to CONSTANT)
   Q_PROPERTY(qreal amount READ amount CONSTANT)
   Q_PROPERTY(QString symbol READ symbol CONSTANT)
   Q_PROPERTY(qreal fee READ fee CONSTANT)
   Q_PROPERTY(QString feeSymbol READ feeSymbol CONSTANT)
   Q_PROPERTY(QString memo READ memo CONSTANT)
   Q_PROPERTY(QString status READ status CONSTANT)

public:
   LightTransactionSummary(QObject* parent = nullptr) :QObject(parent),m_amount(0),m_fee(0){}

   QDateTime when() const
   {
      return m_when;
   }
   QString from() const
   {
      return m_from;
   }
   QString to() const
   {
      return m_to;
   }
   qreal amount() const
   {
      return m_amount;
   }
   QString symbol() const
   {
      return m_symbol;
   }
   qreal fee() const
   {
      return m_fee;
   }
   QString feeSymbol() const
   {
      return m_feeSymbol;
   }
   QString memo() const
   {
      return m_memo;
   }
   QString status() const
   {
      return m_status;
   }

private:
   const QDateTime m_when;
   const QString m_from;
   const QString m_to;
   const qreal m_amount;
   const QString m_symbol;
   const qreal m_fee;
   const QString m_feeSymbol;
   const QString m_memo;
   const QString m_status;
};

class Balance : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString symbol MEMBER m_symbol NOTIFY symbolChanged)
   Q_PROPERTY(qreal amount MEMBER m_amount NOTIFY amountChanged)

   QString m_symbol;
   qreal m_amount;

public:
   Balance(QString symbol, qreal amount, QObject* parent = nullptr)
      : QObject(parent),
        m_symbol(symbol),
        m_amount(amount)
   {}
   virtual ~Balance(){}
Q_SIGNALS:
   void symbolChanged(qreal arg);
   void amountChanged(qreal arg);
};
