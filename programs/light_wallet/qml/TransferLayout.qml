import QtQuick 2.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import Material 0.1

Page {
   id: transferPage
   title: qsTr("Send Payment")
   actions: [lockAction]

   property string accountName
   property string assetSymbol: wallet.accounts[accountName].availableAssets[0]

   signal transferComplete(string assetSymbol)

   RowLayout {
      id: hashBar
      anchors.top: parent.top
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.margins: visuals.margins
      width: Math.min(parent.width - visuals.margins*2, units.dp(600))

      Identicon {
         id: senderHash
         name: wallet.accounts[accountName].name
         x: visuals.margins
         Layout.fillWidth: true
         Layout.preferredWidth: hashBar.width / 2.5
      }
      Identicon {
         id: recipientHash
         name: toNameField.text ? toNameField.text.indexOf(wallet.keyPrefix) === 0 ? "Unregistered Account"
                                                                                   : toNameField.text
                                : "Unknown"
         Layout.fillWidth: true
         Layout.preferredWidth: hashBar.width / 2.5
      }
   }
   Label {
      id: amountLabel
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.top: hashBar.bottom
      style: "display1"
      color: "red"
      text: format(amountField.amount(), assetSymbolField.selectedText) + " " + assetSymbolField.selectedText
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
         bottom: parent.bottom
         horizontalCenter: parent.horizontalCenter
         margins: visuals.margins
      }
      width: Math.min(parent.width - visuals.margins*2, units.dp(600))

      TextField {
         id: toNameField
         width: parent.width
         placeholderText: qsTr("Recipient")
         floatingLabel: true
         focus: true
         onEditingFinished: canProceed()
         transform: ShakeAnimation { id: recipientShaker }
         helperText: "Enter username or key"
         input {
            inputMethodHints: Qt.ImhLatinOnly
            font.pixelSize: units.dp(20)
         }

         function canProceed() {
            if( wallet.accountExists(text) || wallet.isValidKey(text) )
            {
               hasError = false
               helperText = ""
            } else {
               hasError = true
               helperText = text.indexOf(wallet.keyPrefix) === 0 ? qsTr("Invalid recipient key")
                                                                 : qsTr("This account does not exist")
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
               validator: DoubleValidator {bottom: 0; notation: DoubleValidator.StandardNotation; decimals: wallet.getDigitsOfPrecision(assetSymbolField.selectedText)}
            }
            helperText: {
               var fee = wallet.getFee(assetSymbolField.selectedText)
               var balance = Number(wallet.accounts[accountName].balance(assetSymbolField.selectedText).total)
               return qsTr("Available balance: ") + format(balance, assetSymbolField.selectedText) +
                     " " + assetSymbolField.selectedText + "\n" + qsTr("Transaction fee: ") + format(fee.amount, fee.symbol) + " " + fee.symbol
            }
            onEditingFinished: canProceed()

            function amount() {
               return Number.fromLocaleString(text)? Number.fromLocaleString(text) : 0
            }
            function canProceed() {
               var fee = wallet.getFee(assetSymbolField.selectedText)
               var total = amount()
               if( fee.symbol === assetSymbolField.selectedText ) {
                  total += fee.amount
               }

               hasError = total > wallet.accounts[accountName].balance(assetSymbolField.selectedText).total
               return !hasError && amount() > 0
            }
            function canPayFee() {
               var fee = wallet.getFee(assetSymbolField.selectedText)
               if( fee.symbol === assetSymbolField.selectedText )
                  return canProceed()
               console.log("BTS balance: " + wallet.accounts[accountName].balance(fee.symbol).total + fee.symbol)
               return fee.amount <= wallet.accounts[accountName].balance(fee.symbol).total
            }
         }
         MenuField {
            id: assetSymbolField
            Layout.preferredHeight: amountField.height
            anchors.verticalCenter: amountField.verticalCenter
            model: wallet.accounts[accountName].availableAssets
            selectedIndex: model.indexOf(assetSymbol)
            floatingLabel: true
            placeholderText: "Asset"
            hasHelperText: true

            Behavior on width { NumberAnimation {} }
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
               return badFeeDialog.show()
            if( memoField.characterCount > memoField.characterLimit )
               return memoShaker.shake()

            transactionPreview.trx = wallet.accounts[accountName].beginTransfer(toNameField.text, amountField.amount(),
                                                                                assetSymbolField.selectedText, memoField.text);
            transferPage.state = "confirmation"
            confirmPassword.forceActiveFocus()
         }
      }
   }

   Dialog {
      id: badFeeDialog
      title: qsTr("Cannot pay fee")
      height: units.dp(200)
      negativeButtonText: ""
      positiveButtonText: qsTr("OK")

      onAccepted: close()

      Label {
         id: dialogContentLabel
         style: "body1"
         text: qsTr("The BitShares network does not accept fees in " + assetSymbolField.selectedText + ", so the transaction fee " +
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
                  transferComplete(assetSymbolField.selectedText)
               else
                  confirmPassword.shake()
            }
         }
      }
   ]
}
