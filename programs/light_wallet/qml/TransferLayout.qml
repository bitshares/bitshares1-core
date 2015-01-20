import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import Material 0.1

Page {
   title: "Transfer"
   
   RowLayout {
      id: hashBar
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right:  parent.right
      anchors.margins: visuals.margins

      RoboHash {
         name: wallet.account.name
         x: visuals.margins
         Layout.fillWidth: true
      }
      RoboHash {
         id: recipientHash
         name: toNameField.text? toNameField.text : "Unknown"
         Layout.fillWidth: true
      }
   }
   Label {
      id: amountLabel
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.top: hashBar.bottom
      style: "display1"
      color: "red"
      text: Number(amountField.text) + " " + assetSymbol.text
   }

   Rectangle {
      id: splitter
      color: "darkgrey";
      height: 1;
      width: parent.width;
      y: amountLabel.y + amountLabel.height + visuals.margins
   }

   Item {
      id: transferForm
      anchors {
         top: splitter.bottom
         left: parent.left
         right: parent.right
         bottom: parent.bottom
         margins: visuals.margins
      }

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
         text: {
            var fee = wallet.getFee(assetSymbol.text)
            return qsTr("Available balance: ") + wallet.account.balance(assetSymbol.text) + " " + assetSymbol.text +
                  "\n" + qsTr("Transaction fee: ") + fee.amount + " " + fee.symbol
         }
      }
      TextField {
         id: memoField
         width: parent.width
         maximumLength: wallet.maxMemoSize
         placeholderText: qsTr("Memo")
         floatingLabel: true
         font.pixelSize: units.dp(20)
         anchors.top: amountRow.bottom
         anchors.margins: units.dp(3)
      }
   }
   Item {
      id: confirmForm
      anchors {
         top: splitter.bottom
         left: parent.left
         right: parent.right
         bottom: parent.bottom
         margins: visuals.margins
      }
      visible: false


   }

   Row {
      anchors {
         right: parent.right
         bottom: parent.bottom
         margins: visuals.margins
      }
      spacing: visuals.margins

      Button {
         text: qsTr("Cancel")
         onClicked: pop()
      }
      Button {
         text: qsTr("Send")
      }
   }

   states: [
      State {
         name: "confirmation"

      }

   ]
}
