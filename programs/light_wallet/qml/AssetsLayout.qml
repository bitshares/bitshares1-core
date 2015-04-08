import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

import Material 0.1

import "utils.js" as Utils

Page {
   title: wallet.accounts[accountName].name + qsTr("'s Balances")
   actions: [/*__marketAction, DISABLED FOR RELEASE*/ payAction, lockAction]

   Action {
      id: __marketAction
      name: qsTr("Place Market Order")
      iconName: "action/shopping_basket"
      onTriggered: openOrderForm({accountName: accountName})
   }

   property string accountName

   signal lockRequested
   signal openHistory(string account, string symbol)

   ColumnLayout {
      id: assetsLayout
      anchors.top: parent.top
      anchors.bottom: parent.bottom
      anchors.bottomMargin: visuals.margins
      width: parent.width

      ScrollView {
         id: assetList
         Layout.fillWidth: true
         Layout.fillHeight: true
         flickableItem.interactive: true

         ListView {
            model: wallet.accounts[accountName].balances
            delegate: Rectangle {
               width: parent.width
               height: assetRow.height + visuals.margins
               color: index % 2? "transparent" : "#11000000"

               Rectangle { width: parent.width; height: 1; color: "darkgrey"; visible: index }
               RowLayout {
                  id: assetRow
                  width: parent.width
                  anchors.verticalCenter: parent.verticalCenter

                  Item { Layout.preferredWidth: visuals.margins }
                  Label {
                     text: format(amount, symbol) + (yield ? "\n+ " + format(yield, symbol) + qsTr(" yield")
                                                           : "")
                     font.pixelSize: units.dp(32)
                  }
                  Item { Layout.fillWidth: true }
                  Label {
                     text: symbol
                     font.pixelSize: units.dp(32)
                  }
                  Item { Layout.preferredWidth: visuals.margins }
                  Icon {
                     name: "navigation/chevron_right"
                     anchors.verticalCenter: parent.verticalCenter
                     size: units.dp(36)
                  }
               }
               Ink {
                  anchors.fill: parent
                  onClicked: {
                     openHistory(accountName, symbol)
                     console.log("Open trx history for " + accountName + "/" + symbol)
                  }
               }
            }

            PullToRefresh {
               view: parent
               text: qsTr("Release to refresh balances")

               onTriggered: {
                  if( !wallet.connected ) {
                     showMessage(qsTr("Cannot refresh balances: not connected to server."))
                     return
                  }
                  showMessage(qsTr("Refreshing balances"))
                  wallet.syncAllBalances()
               }
            }
         }
      }
   }
}
