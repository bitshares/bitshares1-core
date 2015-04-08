#include "QtWrappers.hpp"
#include "QtConversions.hpp"

#include <bts/blockchain/time.hpp>

#include <boost/random.hpp>

#include <QTimer>

TransactionSummary::TransactionSummary(QString id, fc::time_point_sec timestamp, QList<LedgerEntry*>&& ledger, QObject* parent)
   : QObject(parent),
     m_id(id),
     m_timestamp(timestamp),
     m_ledger(ledger)
{
   for( auto entry : m_ledger )
      entry->setParent(this);

   connect(this, &TransactionSummary::updatingTimestampChanged, [this] (bool updating) {
      if( updating )
         Q_EMIT timestampChanged();
   });
}

QString TransactionSummary::timeString() const
{
   using bts::blockchain::now;

   //Add a random delay, so all the labels don't update at the same time.
   static boost::random::mt19937 generator(time(nullptr));
   boost::random::uniform_int_distribution<> distribution(0, 5000);

   auto age = now() - m_timestamp;

   //There's really no good way to translate these... particularly the last one.
   if( age <= fc::seconds(20) )
   {
      if( m_updatingTimestamp )
         QTimer::singleShot(20000 + distribution(generator), this, &TransactionSummary::timestampChanged);
      return "Just now";
   }

   if( age <= fc::minutes(1) )
   {
      if( m_updatingTimestamp )
         QTimer::singleShot((m_timestamp + fc::seconds(61) - now()).count() / 1000,
                            this, &TransactionSummary::timestampChanged);
      return "Less than a minute ago";
   }

   if( m_updatingTimestamp )
      QTimer::singleShot(60000 + distribution(generator), this, &TransactionSummary::timestampChanged);

   if( age <= fc::minutes(2) )
      return "A minute ago";
   return convert(fc::get_approximate_relative_time_string(m_timestamp, now()));
}
