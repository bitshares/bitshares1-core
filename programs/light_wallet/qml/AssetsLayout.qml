import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import "utils.js" as Utils

Item {
   property real minimumWidth: assetsLayout.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: assetsHeader.height + Math.max(noAssetsText.implicitHeight,
                                                               assetsLayout.Layout.minimumHeight)
                                + visuals.margins * 2

   signal lockRequested

   ColumnLayout {
      id: assetsLayout
      anchors.top: parent.top
      anchors.bottom: parent.bottom
      anchors.bottomMargin: visuals.margins
      width: parent.width - visuals.margins * 2
      x: visuals.margins

      ScrollView {
         id: assetList
         Layout.fillWidth: true
         Layout.fillHeight: true
         flickableItem.interactive: true
         // @disable-check M16 -- For some reason, QtC doesn't recognize this property...
         verticalScrollBarPolicy: Qt.platform.os in ["android", "ios"]? Qt.ScrollBarAsNeeded : Qt.ScrollBarAlwaysOff

         ListView {
            spacing: visuals.spacing / 4
            model: wallet.balances
            delegate: RowLayout {
               width: parent.width
               Label {
                  color: visuals.textColor
                  text: symbol
                  font.pixelSize: visuals.textBaseSize * 2
               }
               Item { Layout.fillWidth: true }
               Label {
                  color: visuals.textColor
                  text: amount
                  font.pixelSize: visuals.textBaseSize * 2
               }
            }
         }
      }
   }
   Label {
      id: noAssetsText
      anchors.centerIn: assetsLayout
      width: parent.width - visuals.margins * 2
      color: visuals.lightTextColor
      font.pixelSize: visuals.textBaseSize * 2
      text: qsTr("You don't have any assets yet. When you do, they'll be listed here.")
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      verticalAlignment: Text.AlignVCenter
      visible: wallet.balances.length === 0
      opacity: .5
      z: -1
   }
}
