import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

import Material 0.1

import "utils.js" as Utils

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
            property string name: incoming? sender : receiver

            Item { Layout.preferredWidth: visuals.margins }
            Identicon {
               name: parent.name
               elideMode: Text.ElideNone
               Layout.preferredWidth: units.dp(100)
            }
            Label {
               Layout.fillWidth: true
               anchors.top: parent.top
               text: memo? memo : name === "Unregistered Account" ? qsTr("Cannot recover memo to unregistered account")
                                                                  : qsTr("No memo provided")
               font.italic: !memo
               font.pixelSize: units.dp(20)
            }
            Column {
               Label {
                  text: format(amount, symbol) + " " + symbol
                  color: incoming? (sender === accountName? "black"
                                                          : "green")
                                 : "red"
                  font.pixelSize: units.dp(32)
               }
               Label {
                  text: qsTr("Yield %1").arg(format(yield, symbol))
                  color: "green"
                  font.pixelSize: units.dp(16)
                  visible: sender === accountName && yield
               }
            }
            Item { Layout.preferredWidth: visuals.margins }
         }
      }

      Item { Layout.preferredHeight: visuals.margins/4 }
      RowLayout {
         Layout.fillWidth: true

         Item { Layout.preferredWidth: visuals.margins }
         Label {
            id: timestampLabel
            text: timestampHoverBox.containsMouse? trx.timestamp.toLocaleString() : trx.timeString
            font.pixelSize: units.dp(12)

            Behavior on text {
               SequentialAnimation {
                  PropertyAnimation {
                     target: timestampLabel
                     property: "opacity"
                     from: 1; to: 0
                  }
                  PropertyAction { target: timestampLabel; property: "text" }
                  PropertyAnimation {
                     target: timestampLabel
                     property: "opacity"
                     from: 0; to: 1
                  }
               }
            }
            MouseArea {
               id: timestampHoverBox
               anchors.fill: parent
               anchors.margins: units.dp(-10)
               hoverEnabled: true
               acceptedButtons: Qt.NoButton
            }
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
   Ink {
      id: transactionInk

      hoverEnabled: false
      anchors.fill: parent
      onClicked: {
         //Iterate ledger entries looking for one which contains the mouse
         for( var index in ledgerRepeater.model ) {
            var entry = ledgerRepeater.itemAt(index)
            var position = entry.mapFromItem(transactionInk, mouse.x, mouse.y)

            //If this entry contains the mouse, or there's only one entry, copy the name
            if( entry.contains(Qt.point(position.x, position.y)) || ledgerRepeater.count === 1 ) {
               Utils.copyTextToClipboard(entry.name)
               showMessage("Copied <i>" + entry.name + "</i> to clipboard")
               return
            }
         }
      }
   }
}
