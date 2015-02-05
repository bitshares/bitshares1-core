import QtQuick 2.3
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Page {
   id: historyPage
   title: accountName + "'s " + assetSymbol + " " + qsTr("Transactions")
   actions: [__payAction, lockAction]
   clip: true

   Action {
      id: __payAction
      name: qsTr("Send Payment")
      iconName: "action/payment"
      onTriggered: openTransferPage({"accountName": accountName, "assetSymbol": assetSymbol})
   }

   property string accountName
   property string assetSymbol

   ScrollView {
      id: historyScroller
      anchors.top: parent.top
      anchors.bottom: balanceBar.top
      anchors.topMargin: visuals.margins
      anchors.bottomMargin: visuals.margins
      width: parent.width
      flickableItem.interactive: true
      // @disable-check M16
      verticalScrollBarPolicy: Qt.platform.os in ["android", "ios"]? Qt.ScrollBarAsNeeded : Qt.ScrollBarAlwaysOff
      viewport.clip: false

      ListView {
         id: historyList
         model: wallet.accounts[accountName].transactionHistory(assetSymbol)
         spacing: visuals.margins / 2
         onDragEnded: {
            if( contentY < units.dp(-100) )
            {
               showError(qsTr("Refreshing transactions"))
               wallet.syncAllTransactions()
            }
         }

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

         Label {
            y: units.dp(-100) - height - parent.contentY
            text: qsTr("Release to refresh transactions")
            anchors.horizontalCenter: parent.horizontalCenter
            style: "headline"
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
         text: getLabel()

         function getLabel() {
            var balance = wallet.accounts[accountName].balance(assetSymbol)
            return format(balance.amount, assetSymbol) + " " + assetSymbol +
                  (balance.yield ? "\n+ " + format(balance.yield, assetSymbol) + qsTr(" yield")
                                 : "")
         }

         color: "white"
         font.pixelSize: units.dp(24)

         Connections {
            target: wallet
            onSynced: balanceLabel.text = balanceLabel.getLabel()
         }
      }
   }
}
