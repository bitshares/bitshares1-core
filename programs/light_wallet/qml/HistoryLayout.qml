import QtQuick 2.3
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Page {
   id: historyPage
   title: accountName + "'s " + assetSymbol + " " + qsTr("Transactions")
   actions: [payAction, lockAction]
   clip: true

   property string accountName
   property string assetSymbol

   ScrollView {
      id: historyScroller
      anchors.fill: parent
      anchors.topMargin: visuals.margins
      anchors.bottomMargin: balanceBar.height + visuals.margins
      flickableItem.interactive: true
      // @disable-check M16
      verticalScrollBarPolicy: Qt.platform.os in ["android", "ios"]? Qt.ScrollBarAsNeeded : Qt.ScrollBarAlwaysOff
      viewport.clip: false

      ListView {
         id: historyList
         model: wallet.accounts[accountName].transactionHistory(assetSymbol)
         spacing: visuals.margins / 2

         Connections {
            target: wallet
            onSynced: historyList.model = wallet.accounts[accountName].transactionHistory(assetSymbol)
         }

         delegate: Transaction {
            width: parent.width - visuals.margins * 2
            anchors.horizontalCenter: parent.horizontalCenter
            trx: model.modelData
            accountName: historyPage.accountName
         }
      }
   }
   View {
      id: balanceBar
      anchors.bottom: parent.bottom
      height: balanceLabel.height + visuals.margins*2
      width: parent.width
      backgroundColor: Theme.primaryColor
      elevation: 2
      z: 2
      elevationInverted: true

      Label {
         anchors.verticalCenter: parent.verticalCenter
         x: visuals.margins
         text: qsTr("Balance:")
         color: "white"
         font.pixelSize: units.dp(24)
      }
      Label {
         id: balanceLabel
         anchors.verticalCenter: parent.verticalCenter
         anchors.right: parent.right
         anchors.rightMargin: visuals.margins
         text: wallet.accounts[accountName].balance(assetSymbol) + " " + assetSymbol
         color: "white"
         font.pixelSize: units.dp(24)

         Connections {
            target: wallet
            onSynced: balanceLabel.text = wallet.accounts[accountName].balance(assetSymbol) + " " + assetSymbol
         }
      }
   }
}
