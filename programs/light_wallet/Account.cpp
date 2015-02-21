#include <bts/blockchain/types.hpp>

#include <QStringList>

#include "Account.hpp"
#include "QtConversions.hpp"

using std::string;

TransactionSummary* Account::buildSummary(bts::wallet::transaction_ledger_entry trx)
{
   QList<LedgerEntry*> ledger;
   wdump((trx));

   for( auto label : trx.delta_labels )
   {
      QStringList participants = convert(label.second).split('>');
      if( participants.size() == 1 ) participants.prepend(tr("Unknown"));

      for( bts::blockchain::asset amount : trx.delta_amounts[label.second] )
      {
         auto asset = m_wallet->get_asset_record(amount.asset_id);
         if( !asset ) continue;

         LedgerEntry* entry = new LedgerEntry();

         entry->setProperty("sender", participants.first());
         entry->setProperty("receiver", participants.last());
         entry->setProperty("amount", double(amount.amount) / asset->precision);
         entry->setProperty("symbol", convert(asset->symbol));
         if( trx.operation_notes.count(label.first) )
            entry->setProperty("memo", convert(trx.operation_notes[label.first]));

         auto& yields = trx.delta_amounts["Yield"];
         auto asset_yield = std::find_if(yields.begin(), yields.end(),
                                         [amount] (const bts::blockchain::asset& a) {
                                            return a.asset_id == amount.asset_id;
                                         });
         if( asset_yield != yields.end() )
            entry->setProperty("yield", double(asset_yield->amount) / asset->precision);

         ledger.append(entry);
      }
   }

   TransactionSummary* summary = new TransactionSummary(convert(string(trx.id)),
                                                        trx.timestamp,
                                                        std::move(ledger), this);

   if( trx.delta_amounts.count("Fee") && trx.delta_amounts["Fee"].size() )
   {
      auto fee = trx.delta_amounts["Fee"].front();
      if( bts::blockchain::oasset_record record = m_wallet->get_asset_record(fee.asset_id) )
      {
         summary->setProperty("feeAmount", convert(record->amount_to_string(fee.amount, false)).remove(QRegExp("0*$")));
         summary->setProperty("feeSymbol", convert(record->symbol));
      }
   }

   summary->setProperty("timestamp", convert(fc::get_approximate_relative_time_string(trx.timestamp)));

   return summary;
}

Account::Account(bts::light_wallet::light_wallet* wallet,
                 const bts::blockchain::account_record& account,
                 QObject* parent)
   : QObject(parent),
     m_wallet(wallet),
     m_name(convert(account.name)),
     m_ownerKey(convert(std::string(account.owner_key))),
     m_activeKey(convert(std::string(account.active_key()))),
     m_isRegistered(account.registration_date != fc::time_point_sec()),
     m_registrationDate(convert(account.registration_date))
{}

Account& Account::operator=(const bts::blockchain::account_record& account)
{
   m_name = convert(account.name);
   m_ownerKey = convert(std::string(account.owner_key));
   m_activeKey = convert(std::string(account.active_key()));
   m_isRegistered = (account.registration_date != fc::time_point_sec());
   m_registrationDate = convert(account.registration_date);
   Q_EMIT nameChanged(m_name);
   Q_EMIT ownerKeyChanged(m_ownerKey);
   Q_EMIT activeKeyChanged(m_activeKey);
   Q_EMIT isRegisteredChanged(m_isRegistered);
   Q_EMIT registrationDateChanged(m_registrationDate);

   return *this;
}

QQmlListProperty<Balance> Account::balances()
{
   auto count = [](QQmlListProperty<Balance>* list) -> int {
      return static_cast<QList<Balance*>*>(list->data)->size();
   };
   auto at = [](QQmlListProperty<Balance>* list, int index) -> Balance* {
      return static_cast<QList<Balance*>*>(list->data)->at(index);
   };

   while( !balanceList.empty() )
      balanceList.takeFirst()->deleteLater();
   for( auto balance : m_wallet->balance(convert(m_name)) )
      balanceList.append(new Balance(convert(balance.first), balance.second.first, balance.second.second, this));
   return QQmlListProperty<Balance>(this, &balanceList, count, at);
}

Balance* Account::balance(QString symbol)
{
   return new Balance(symbol, m_wallet->balance(convert(m_name))[convert(symbol)]);
}

QStringList Account::availableAssets()
{
   QStringList assets;
   for( auto balance : m_wallet->balance(convert(m_name)) )
      if( balance.second.first > 0 || balance.second.second > 0 )
         assets.append(convert(balance.first));
   return assets;
}

QList<QObject*> Account::transactionHistory(QString asset_symbol)
{
   try {
      auto raw_transactions = m_wallet->transactions(convert(m_name), convert(asset_symbol));
      std::sort(raw_transactions.rbegin(), raw_transactions.rend(),
                [](const bts::wallet::transaction_ledger_entry& a,
                const bts::wallet::transaction_ledger_entry& b) {
         return a.timestamp < b.timestamp;
      });

      int position = 0;
      for( bts::wallet::transaction_ledger_entry trx : raw_transactions )
      {
         if( std::find_if(transactionList[asset_symbol].begin(), transactionList[asset_symbol].end(),
                          [&trx] (QObject* o) -> bool {
                             return o->property("id").toString() == convert(string(trx.id));
                          }) != transactionList[asset_symbol].end() )
            continue;

         transactionList[asset_symbol].insert(position++, buildSummary(std::move(trx)));
      }
   } catch( fc::exception& e ) {
      elog("Unhandled exception: ${e}", ("e", e.to_detail_string()));
      Q_EMIT error(tr("Internal error while building transaction history"));
   }

   return transactionList[asset_symbol];
}

TransactionSummary* Account::beginTransfer(QString recipient, QString amount, QString symbol, QString memo)
{
   try {
      pending_transaction = m_wallet->prepare_transfer(convert(amount), convert(symbol), convert(m_name),
                                                       convert(recipient), convert(memo));
   } catch (const fc::exception& e) {
      qDebug() << "Failed to transfer:" << e.to_detail_string().c_str();
      return nullptr;
   }

   auto summary = buildSummary(m_wallet->summarize(convert(m_name), pending_transaction));
   summary->setParent(nullptr);
   summary->setProperty("timestamp", QStringLiteral("Pending"));
   connect(summary, &QObject::destroyed, [] {
      qDebug() << "Summary cleaned";
   });
   return summary;
}

bool Account::completeTransfer(QString password)
{
   try {
      return m_wallet->complete_transfer(convert(m_name), convert(password), pending_transaction);
   } catch( const fc::exception& e ) {
      if( e.get_log().size() )
         Q_EMIT error(convert(e.get_log().front().get_message()));
      else
         Q_EMIT error(QStringLiteral("Unknown error occurred."));

      pending_transaction = fc::variant_object();
   }
   return false;
}
