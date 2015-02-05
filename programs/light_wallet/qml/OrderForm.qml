import QtQuick 2.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.3
import QtGraphicalEffects 1.0

import Qt.labs.settings 1.0

import "utils.js" as Utils
import org.BitShares.Types 1.0

import Material 0.1
import Material.ListItems 0.1

Page {
   title: qsTr("Market Order")
   actions: [lockAction]
   
   property string accountName
   
   Flickable {
      anchors.fill: parent

      Column {
         width: units.dp(600)
         y: units.dp(16)
         
         Row {
            width: parent.width - visuals.margins*2
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: visuals.margins
            
            MenuField {
               id: orderTypeField
               model: ["Buy", "Sell", "Short"]

               Behavior on width { NumberAnimation {} }
            }
            MenuField {
               id: orderFirstAssetField
               model: wallet.accounts[accountName].availableAssets

               Behavior on width { NumberAnimation {} }
            }
            Label {
               text: orderTypeField.selectedText === "Short"? qsTr("with") : qsTr("for")
               horizontalAlignment: Text.AlignHCenter
               anchors.verticalCenter: parent.verticalCenter
               style: "subheading"

               Behavior on width { NumberAnimation {} }
            }
            MenuField {
               id: orderSecondAssetField
               model: wallet.accounts[accountName].availableAssets

               Behavior on width { NumberAnimation {} }
            }
         }
      }
   }
}
