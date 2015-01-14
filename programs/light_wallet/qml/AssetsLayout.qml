import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

import "utils.js" as Utils

Item {
   property real minimumWidth: assetsLayout.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: header.height + assetsLayout.Layout.minimumHeight + visuals.margins * 2

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
         // @disable-check M16 -- For some reason, QtC doesn't recognize this property...
         verticalScrollBarPolicy: Qt.platform.os in ["android", "ios"]? Qt.ScrollBarAsNeeded : Qt.ScrollBarAlwaysOff

         ListView {
            model: wallet.account.balances
            delegate: Rectangle {
               width: parent.width
               height: assetRow.height + visuals.spacing/2
               color: index % 2? "transparent" : "#11000000"

               Rectangle { width: parent.width; height: 1; color: "darkgrey"; visible: index }
               RowLayout {
                  id: assetRow
                  width: parent.width
                  anchors.verticalCenter: parent.verticalCenter

                  Item { Layout.preferredWidth: visuals.margins }
                  Label {
                     color: visuals.textColor
                     text: amount
                     font.pixelSize: visuals.textBaseSize * 2
                  }
                  Item { Layout.fillWidth: true }
                  Label {
                     color: visuals.textColor
                     text: symbol
                     font.pixelSize: visuals.textBaseSize * 2
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
                     openHistory(wallet.account.name, symbol)
                     console.log("Open trx history for " + wallet.account.name + "/" + symbol)
                  }
               }
            }
         }
      }
   }
}
