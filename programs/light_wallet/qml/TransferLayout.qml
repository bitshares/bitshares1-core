import QtQuick 2.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import Material 0.1

Page {
   id: transferPage
   title: qsTr("Send Payment")
   actions: [lockAction]

   property string accountName

   signal transferComplete(string assetSymbol)
   
   RowLayout {
      id: hashBar
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right:  parent.right
      anchors.margins: visuals.margins

      RoboHash {
         name: wallet.accounts[accountName].name
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
      text: amountField.amount() + " " + assetSymbol.text
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
         placeholderText: qsTr("Recipient")
         floatingLabel: true
         focus: true
         onEditingFinished: canProceed()
         transform: ShakeAnimation { id: recipientShaker }
         helperText: " "
         input {
            inputMethodHints: Qt.ImhLowercaseOnly | Qt.ImhLatinOnly
            font.pixelSize: units.dp(20)
         }

         function canProceed() {
            if( !wallet.accountExists(text) )
            {
               hasError = true
               helperText = qsTr("This account does not exist")
            } else {
               hasError = false
               helperText = ""
            }

            return !hasError
         }
      }
      RowLayout {
         id: amountRow
         z: 2
         anchors.top: toNameField.bottom
         anchors.topMargin: units.dp(3)
         width: parent.width
         transform: ShakeAnimation { id: amountShaker }

         TextField {
            id: amountField
            Layout.fillWidth: true
            placeholderText: qsTr("Amount")
            floatingLabel: true
            input {
               inputMethodHints: Qt.ImhDigitsOnly
               font.pixelSize: units.dp(20)
               validator: DoubleValidator {bottom: 0; notation: DoubleValidator.StandardNotation}
            }
            helperText: {
               var fee = wallet.getFee(assetSymbol.text)
               return qsTr("Available balance: ") + wallet.accounts[accountName].balance(assetSymbol.text) +
                     " " + assetSymbol.text + "\n" + qsTr("Transaction fee: ") + fee.amount + " " + fee.symbol
            }
            onEditingFinished: canProceed()

            function amount() {
               return Number(text)? Number(text) : 0
            }
            function canProceed() {
               var fee = wallet.getFee(assetSymbol.text)
               var total = amount()
               if( fee.symbol === assetSymbol.text ) {
                  total += fee.amount
               }

               hasError = total > wallet.accounts[accountName].balance(assetSymbol.text)
               return !hasError && amount() > 0
            }
            function canPayFee() {
               var fee = wallet.getFee(assetSymbol.text)
               if( fee.symbol === assetSymbol.text )
                  return canProceed()
               return fee.amount <= wallet.accounts[accountName].balance(fee.symbol)
            }
         }
         TextField {
            id: assetSymbol
            y: amountField.y
            Layout.preferredWidth: units.dp(100)
            input.readOnly: true
            input.font.pixelSize: units.dp(20)
            //Hack to make RowLayout place this field vertically level with amountField
            helperText: "\n "
            text: wallet.accounts[accountName].availableAssets.length ? wallet.accounts[accountName].availableAssets[0]
                                                        : ":("
            placeholderText: qsTr("Asset")
            floatingLabel: true

            Icon {
               anchors.verticalCenter: parent.verticalCenter
               anchors.right: parent.right
               anchors.rightMargin: units.dp(5)
               name: "navigation/arrow_drop_down"
            }
            Ink {
               anchors.fill: parent
               enabled: !assetMenu.opened
               onClicked: assetMenu.open(assetSymbol.text, assetSymbol.inputRect.x, assetSymbol.inputRect.y)
            }
            Menu {
               id: assetMenu
               width: parent.width * 1.1
               model: wallet.accounts[accountName].availableAssets
               owner: assetSymbol
               onElementSelected: assetSymbol.text = elementData
            }
         }
      }
      TextField {
         id: memoField
         width: parent.width
         characterLimit: wallet.maxMemoSize
         placeholderText: qsTr("Memo")
         floatingLabel: true
         input.font.pixelSize: units.dp(20)
         anchors.top: amountRow.bottom
         anchors.margins: units.dp(3)
         transform: ShakeAnimation { id: memoShaker }
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

      Transaction {
         id: transactionPreview
         width: parent.width
         anchors.horizontalCenter: parent.horizontalCenter
         accountName: accountName
      }

      PasswordField {
         id: confirmPassword
         placeholderText: qsTr("Enter password to confirm transaction")
         floatingLabel: false
         width: parent.width
         anchors.horizontalCenter: parent.horizontalCenter
         anchors.top: transactionPreview.bottom
         anchors.topMargin: visuals.margins
         onAccepted: continueButton.clicked()
      }
   }

   Row {
      anchors {
         right: parent.right
         bottom: parent.bottom
         margins: visuals.margins
      }
      spacing: visuals.margins

      Button {
         id: cancelButton
         text: qsTr("Cancel")
         onClicked: pop()
      }
      Button {
         id: continueButton
         text: qsTr("Send")
         onClicked: {
            if( !toNameField.canProceed() )
               return recipientShaker.shake()
            if( !amountField.canProceed() )
               return amountShaker.shake()
            if( !amountField.canPayFee() )
               return badFeeDialog.open()
            if( memoField.characterCount > memoField.characterLimit )
               return memoShaker.shake()

            transactionPreview.trx = wallet.accounts[accountName].beginTransfer(toNameField.text, amountField.text,
                                                                                assetSymbol.text, memoField.text);
            transferPage.state = "confirmation"
            confirmPassword.forceActiveFocus()
         }
      }
   }

   MouseArea {
      anchors.fill: parent
      onClicked: badFeeDialog.close()
      enabled: badFeeDialog.visible
   }
   Dialog {
      id: badFeeDialog
      title: qsTr("Cannot pay fee")
      height: units.dp(200)
      negativeBtnText: ""
      positiveBtnText: qsTr("OK")
      visible: false

      onAccepted: close()

      Label {
         id: dialogContentLabel
         style: "body1"
         text: qsTr("The BitShares network does not accept fees in " + assetSymbol.text + ", so the transaction fee " +
                    "must be paid in " + wallet.baseAssetSymbol + ". You do not currently have enough " +
                    wallet.baseAssetSymbol + " to pay the fee.")
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
         width: parent.width
      }
   }

   states: [
      State {
         name: "confirmation"
         PropertyChanges {
            target: transferForm
            visible: false
         }
         PropertyChanges {
            target: confirmForm
            visible: true
         }
         PropertyChanges {
            target: cancelButton
            onClicked: transferPage.state = ""
         }
         PropertyChanges {
            target: continueButton
            text: qsTr("Confirm")
            onClicked: {
               if( wallet.accounts[accountName].completeTransfer(confirmPassword.password) )
                  transferComplete(assetSymbol.text)
               else
                  confirmPassword.shake()
            }
         }
      }
   ]
}
