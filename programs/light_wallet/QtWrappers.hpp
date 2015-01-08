#pragma once

#include <QDateTime>
#include <QLatin1String>
#include <QObject>

#include <bts/light_wallet/light_wallet.hpp>

#include "QtConversions.hpp"

#define SETTER_(TYPE, INTERNAL_NAME, EXTERNAL_NAME) \
   void set ## EXTERNAL_NAME(TYPE arg) { \
      auto conv = convert(arg); \
      if( m.INTERNAL_NAME == conv ) return; \
      m.INTERNAL_NAME = conv; \
      Q_EMIT EXTERNAL_NAME ## Changed(arg); \
   }
#define SETTER(TYPE, NAME) SETTER_(TYPE, NAME, NAME)
#define GETTER_(TYPE, INTERNAL_NAME, EXTERNAL_NAME) \
   TYPE EXTERNAL_NAME() { \
      return convert(m.INTERNAL_NAME); \
   }
#define GETTER(TYPE, NAME) GETTER_(TYPE, NAME, NAME)

class LightTransactionSummary : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QDateTime when READ when WRITE setwhen NOTIFY whenChanged)
   Q_PROPERTY(QString from READ from WRITE setfrom NOTIFY fromChanged)
   Q_PROPERTY(QString to READ to WRITE setto NOTIFY toChanged)
   Q_PROPERTY(qreal amount READ amount WRITE setamount NOTIFY amountChanged)
   Q_PROPERTY(QString symbol READ symbol WRITE setsymbol NOTIFY symbolChanged)
   Q_PROPERTY(qreal fee READ fee WRITE setfee NOTIFY feeChanged)
   Q_PROPERTY(QString feeSymbol READ feeSymbol WRITE setfeeSymbol NOTIFY feeSymbolChanged)
   Q_PROPERTY(QString memo READ memo WRITE setmemo NOTIFY memoChanged)
   Q_PROPERTY(QString status READ status WRITE setstatus NOTIFY statusChanged)

public:
   LightTransactionSummary(bts::light_wallet::light_transaction_summary&& summary)
      : m(summary){}

   GETTER(QDateTime, when)
   GETTER(QString, from)
   GETTER(QString, to)
   qreal amount() { return m.amount; }
   GETTER(QString, symbol)
   qreal fee() { return m.fee; }
   GETTER_(QString, fee_symbol, feeSymbol)
   GETTER(QString, memo)
   GETTER(QString, status)

public Q_SLOTS:
   SETTER(QDateTime, when)
   SETTER(QString, from)
   SETTER(QString, to)
   void setamount(qreal newAmount)
   {
      if( newAmount == m.amount ) return;
      m.amount = newAmount;
      Q_EMIT amountChanged(m.amount);
   }
   SETTER(QString, symbol)
   void setfee(qreal newFee)
   {
      if( newFee == m.fee ) return;
      m.fee = newFee;
      Q_EMIT feeChanged(m.fee);
   }
   SETTER_(QString, fee_symbol, feeSymbol)
   SETTER(QString, memo)
   SETTER(QString, status)

Q_SIGNALS:
   void whenChanged(QDateTime arg);
   void fromChanged(QString arg);
   void toChanged(QString arg);
   void amountChanged(qreal arg);
   void symbolChanged(QString arg);
   void feeChanged(qreal arg);
   void feeSymbolChanged(QString arg);
   void memoChanged(QString arg);
   void statusChanged(QString arg);

private:

   bts::light_wallet::light_transaction_summary m;
};

class Account : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
   Q_PROPERTY(bool isRegistered READ isRegistered NOTIFY isRegisteredChanged)
   Q_PROPERTY(QDateTime registrationDate READ registrationDate NOTIFY registrationDateChanged)

   QString m_name;
   bool m_isRegistered;
   QDateTime m_registrationDate;

public:
   Account(QObject* parent = nullptr): QObject(parent){}
   Account(const bts::light_wallet::account_record& account, QObject* parent = nullptr);
   virtual ~Account(){}

   Account& operator= (const bts::light_wallet::account_record& account);

   QString name() const
   {
      return m_name;
   }
   bool isRegistered() const
   {
      return m_isRegistered;
   }
   QDateTime registrationDate() const
   {
      return m_registrationDate;
   }

public Q_SLOTS:
   void setName(QString arg)
   {
      if (m_name == arg)
         return;

      m_name = arg;
      Q_EMIT nameChanged(arg);
   }

Q_SIGNALS:
   void nameChanged(QString arg);
   void isRegisteredChanged(bool arg);
   void registrationDateChanged(QDateTime arg);
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
