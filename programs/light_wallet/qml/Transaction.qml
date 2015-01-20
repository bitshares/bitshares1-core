import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Rectangle {
   width: parent.width - visuals.margins * 2
   height: transactionSummary.height
   anchors.horizontalCenter: parent.horizontalCenter
   radius: units.dp(5)

   property var trx
   
   ColumnLayout {
      id: transactionSummary
      width: parent.width
      
      property string timestamp: trx.modelData.timestamp
      
      Item { Layout.preferredHeight: visuals.margins }
      Repeater {
         id: ledgerRepeater
         model: trx.modelData.ledger
         delegate: RowLayout {
            width: parent.width
            property bool incoming: receiver === accountName
            
            Item { Layout.preferredWidth: visuals.margins }
            RoboHash {
               name: incoming? sender : receiver
               Layout.preferredWidth: units.dp(100)
            }
            Column {
               Layout.fillWidth: true
               Label {
                  text: memo
                  font.pixelSize: units.dp(16)
               }
               Item { width: 1; height: units.dp(8) }
               Label {
                  text: (index === 0)? (transactionSummary.timestamp) : ""
                  font.pixelSize: units.dp(16)
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
      Item { Layout.preferredHeight: visuals.margins }
   }
   Ink { anchors.fill: parent }
}
