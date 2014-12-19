import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import "utils.js" as Utils

Item {
   property real minimumWidth: Math.max(assetHeaderRow.Layout.minimumWidth,
                                        assetsLayout.Layout.minimumWidth)
                               + visuals.margins * 2
   property real minimumHeight: assetsLayout.y + assetsLayout.Layout.minimumHeight + visuals.margins * 2

   signal lockRequested

   Rectangle {
      id: assetsHeader
      width: parent.width
      height: assetHeaderRow.height + visuals.margins * 2
      color: "#22000000"
      
      RowLayout {
         id: assetHeaderRow
         width: parent.width - visuals.margins * 2
         y: visuals.margins
         anchors.horizontalCenter: parent.horizontalCenter
         
         Button {
            style: WalletButtonStyle{}
            text: qsTr("Lock")
            onClicked: lockRequested()
         }
         Item { Layout.fillWidth: true }
         Label {
            color: visuals.textColor
            font.pointSize: visuals.textBaseSize * 1.1
            text: wallet.accountName + qsTr("'s Assets")
         }
         Item { Layout.fillWidth: true }
         Button {
            style: WalletButtonStyle{}
            text: qsTr("Transfer")
         }
      }
      Rectangle { height: 1; width: parent.width; color: visuals.lightTextColor; anchors.bottom: parent.bottom }
   }
   ColumnLayout {
      id: assetsLayout
      anchors.top: assetsHeader.bottom
      anchors.bottom: parent.bottom
      anchors.bottomMargin: visuals.margins
      width: parent.width - visuals.margins * 2
      x: visuals.margins
      
      ScrollView {
         Layout.fillWidth: true
         Layout.fillHeight: true
      }
   }
}
