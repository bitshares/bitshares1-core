import QtQuick 2.3
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Rectangle {
   height: transactionSummary.height
   radius: units.dp(5)

   property var trx: {"timestamp": "Invalid", "ledger": [], "feeAmount": 0}
   property string accountName
   
   ColumnLayout {
      id: transactionSummary
      width: parent.width
      
      property string timestamp: trx.timestamp
      
      Item { Layout.preferredHeight: visuals.margins }
      Repeater {
         id: ledgerRepeater
         model: trx.ledger
         delegate: RowLayout {
            property bool incoming: receiver === accountName
            
            Item { Layout.preferredWidth: visuals.margins }
            RoboHash {
               name: incoming? sender : receiver
               elideMode: Text.ElideNone
               Layout.preferredWidth: units.dp(100)
            }
            Label {
               Layout.fillWidth: true
               anchors.top: parent.top
               text: memo? memo : "No memo provided"
               font.italic: !memo
               font.pixelSize: units.dp(20)
            }
            Label {
               text: format(amount, symbol) + " " + symbol
               color: incoming? (sender === accountName? "black"
                                                        : "green")
                              : "red"
               font.pixelSize: units.dp(32)
            }
            Item { Layout.preferredWidth: visuals.margins }
         }
      }

      Item { Layout.preferredHeight: visuals.margins/4 }
      RowLayout {
         Layout.fillWidth: true

         Item { Layout.preferredWidth: visuals.margins }
         Label {
            text: trx.timestamp
            font.pixelSize: units.dp(12)
         }
         Item { Layout.fillWidth: true }
         Label {
            font.pixelSize: units.dp(10)
            font.weight: Font.Light
            font.italic: true
            visible: trx.feeAmount
            text: qsTr("Fee: ") + format(trx.feeAmount, trx.feeSymbol) + " " + trx.feeSymbol
         }
         Item { Layout.preferredWidth: visuals.margins }
      }
      Item { Layout.preferredHeight: units.dp(3) }
   }
   Ink { anchors.fill: parent }
}
