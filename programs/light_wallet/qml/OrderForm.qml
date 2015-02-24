import QtQuick 2.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2
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

               onItemSelected: {
                  //If user selected asset in other field, and swapping is legal, swap them
                  if( model[index] === orderSecondAssetField.selectedText &&
                        orderSecondAssetField.model.indexOf(selectedText) >= 0 )
                     orderSecondAssetField.selectedIndex = orderSecondAssetField.model.indexOf(selectedText)
               }

               Behavior on width { NumberAnimation {} }
            }
            Label {
               text: orderTypeField.selectedText !== "Sell"? qsTr("with") : qsTr("for")
               horizontalAlignment: Text.AlignHCenter
               anchors.verticalCenter: parent.verticalCenter
               style: "subheading"

               Behavior on width { NumberAnimation {} }
            }
            MenuField {
               id: orderSecondAssetField
               model: wallet.allAssets

               onItemSelected: {
                  //If user selected asset in other field, and swapping is legal, swap them
                  if( model[index] === orderFirstAssetField.selectedText &&
                        orderFirstAssetField.model.indexOf(selectedText) >= 0 )
                     orderFirstAssetField.selectedIndex = orderFirstAssetField.model.indexOf(selectedText)
               }

               Behavior on width { NumberAnimation {} }
            }
         }
      }
   }
}
