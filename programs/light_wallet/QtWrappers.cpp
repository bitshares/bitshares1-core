#include "QtWrappers.hpp"
#include "QtConversions.hpp"

TransactionSummary::TransactionSummary(QString id, QDateTime timestamp, QList<LedgerEntry*>&& ledger, QObject* parent)
   : QObject(parent),
     m_id(id),
     m_when(timestamp),
     m_ledger(ledger)
{}
