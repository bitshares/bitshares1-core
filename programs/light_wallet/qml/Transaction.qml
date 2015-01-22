import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Rectangle {
   height: transactionSummary.height
   radius: units.dp(5)

   property var trx
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
               Layout.preferredWidth: units.dp(100)
            }
            Column {
               Layout.fillWidth: true
               Label {
                  text: memo? memo : "No memo provided"
                  font.italic: !memo
                  font.pixelSize: units.dp(16)
               }
               Item { width: 1; height: units.dp(8) }
               Label {
                  text: (index === 0)? (transactionSummary.timestamp) : ""
                  font.pixelSize: units.dp(12)
               }
            }
            Label {
               text: amount + " " + symbol
               color: incoming? "green" : "red"
               font.pixelSize: units.dp(16)
            }
            Item { Layout.preferredWidth: visuals.margins }
         }
      }
      Label {
         font.pixelSize: units.dp(10)
         font.weight: Font.Light
         font.italic: true
         anchors.right: parent.right
         anchors.rightMargin: visuals.margins
         text: qsTr("Fee: ") + trx.feeAmount + " " + trx.feeSymbol
      }

      Item { Layout.preferredHeight: visuals.margins }
   }
   Ink { anchors.fill: parent }
}
