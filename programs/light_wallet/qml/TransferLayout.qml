import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import Material 0.1

Page {
   title: "Transfer"
   
   Item {
      id: hashBar
      y: visuals.margins
      width: parent.width
      height: amountLabel.y + amountLabel.height + visuals.margins

      RoboHash {
         name: wallet.account.name
         x: visuals.margins
         width: Math.min(units.dp(110), preferredWidth)
      }
      Label {
         id: amountLabel
         anchors.horizontalCenter: parent.horizontalCenter
         anchors.top: recipientHash.bottom
         style: "display1"
         color: "red"
         text: "1,000,000.0001 XTS"
      }
      RoboHash {
         id: recipientHash
         name: toNameField.text? toNameField.text : "Unknown"
         width: Math.min(units.dp(110), preferredWidth)
         anchors.right: parent.right
         anchors.rightMargin: visuals.margins
      }
   }

   Rectangle {
      id: splitter
      color: "darkgrey";
      height: 1;
      width: parent.width;
      y: hashBar.y + hashBar.height + visuals.margins
   }

   Item {
      anchors.top: splitter.bottom
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.bottom: parent.bottom
      anchors.margins: visuals.margins

      TextField {
         id: toNameField
         width: parent.width
         inputMethodHints: Qt.ImhLowercaseOnly | Qt.ImhLatinOnly
         placeholderText: qsTr("Recipient")
         floatingLabel: true
         font.pixelSize: units.dp(20)
         focus: true
      }
      RowLayout {
         id: amountRow
         z: 2
         anchors.top: toNameField.bottom
         anchors.topMargin: units.dp(3)
         width: parent.width

         TextField {
            id: amountField
            Layout.fillWidth: true
            inputMethodHints: Qt.ImhDigitsOnly
            placeholderText: qsTr("Amount")
            floatingLabel: true
            font.pixelSize: units.dp(20)
            validator: DoubleValidator {bottom: 0; notation: DoubleValidator.StandardNotation}
         }
         TextField {
            id: assetSymbol
            y: amountField.y
            Layout.preferredWidth: units.dp(100)
            readOnly: true
            text: "XTS"
            placeholderText: qsTr("Asset")
            floatingLabel: true
            font.pixelSize: units.dp(20)
            cursorPosition: 0

            Icon {
               anchors.verticalCenter: parent.verticalCenter
               anchors.right: parent.right
               anchors.rightMargin: units.dp(5)
               name: "navigation/arrow_drop_down"
            }
            Ink {
               anchors.fill: parent
               enabled: !assetMenu.opened
               onClicked: assetMenu.open(assetSymbol.text, 0, 0)
            }
            Menu {
               id: assetMenu
               width: parent.width * 1.1
               model: wallet.account.availableAssets
               owner: assetSymbol
               onElementSelected: assetSymbol.text = elementData
            }
         }
      }
      Label {
         font.pixelSize: units.dp(12)
         anchors.top: amountRow.bottom
         anchors.right: amountRow.right
         text: qsTr("Available balance: ") + wallet.account.balance(assetSymbol.text) + " " + assetSymbol.text
      }
   }
}
