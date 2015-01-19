import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

import "utils.js" as Utils

MainView {
   id: onboarder

   property real minimumWidth: layout.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: layout.Layout.minimumHeight + visuals.margins * 2
   property alias username: nameField.text
   property string errorMessage

   signal finished

   function registerAccount() {
      onboarder.state = "REGISTERING"
      Utils.connectOnce(wallet.account.isRegisteredChanged, finished,
                        function() { return wallet.account.isRegistered })
      Utils.connectOnce(wallet.onErrorRegistering, function(reason) {
         //FIXME: Do something much, much better here...
         console.log("Can't register: " + reason)
      })

      if( wallet.connected ) {
         wallet.registerAccount()
      } else {
         // Not connected. Schedule for when we reconnect.
         wallet.runWhenConnected(function() {
            wallet.registerAccount()
         })
      }
   }
   function passwordEntered(password) {
      // First time through; there's no wallet, no account, not registered. First, create the wallet.
      if( !wallet.walletExists ) {
         Utils.connectOnce(wallet.walletExistsChanged, function(walletWasCreated) {
            if( walletWasCreated ) {
               registerAccount()
            } else {
               //TODO: failed to create wallet. What now?
               window.showError("Unable to create wallet. Cannot continue.")
               welcomeBox.state = ""
            }
         })
         wallet.createWallet(username, password)
      } else if ( !wallet.account.isRegistered ) {
         //Wallet is created, so the account exists, but it's not registered yet. Take care of that now.
         registerAccount()
      }
   }
   function clearPassword() {
      passwordField.password = ""
   }

   Component.onCompleted: nameField.forceActiveFocus()


   Rectangle {
      anchors.fill: parent
      color: Theme.backgroundColor
   }
   Column {
      id: layout
      anchors.centerIn: parent
      width: parent.width - visuals.margins * 2
      spacing: visuals.margins

      Label {
         anchors.horizontalCenter: parent.horizontalCenter
         horizontalAlignment: Text.AlignHCenter
         text: qsTr("Welcome to BitShares")
         style: "headline"
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      Label {
         id: statusText
         text: qsTr("To get started, create a password below.\n" +
                    "This password can be short and easy to remember — we'll make a better one later.")
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width
         style: "subheading"
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      ColumnLayout {
         width: parent.width

         TextField {
            id: nameField
            inputMethodHints: Qt.ImhLowercaseOnly | Qt.ImhLatinOnly
            placeholderText: qsTr("Pick a Username")
            font.pixelSize: units.dp(20)
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            text: wallet.account? wallet.account.name : ""
         }
         PasswordField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: qsTr("Create a Password")
            fontPixelSize: units.dp(20)
            onAccepted: openButton.clicked()

            Connections {
               target: wallet
               onErrorUnlocking: passwordField.shake()
            }
         }
         Button {
            id: openButton
            text: qsTr("Begin")
            Layout.fillWidth: true
            Layout.preferredHeight: passwordField.height

            onClicked: {
               if( wallet.account )
                  wallet.account.name = nameField.text

               if( passwordField.password.length < 1 ) {
                  passwordField.shake()
               } else {
                  passwordEntered(passwordField.password)
               }
            }
         }
      }
   }

   states: [
      State {
         name: "REGISTERING"
         PropertyChanges {
            target: openButton
            enabled: false
         }
         PropertyChanges {
            target: statusText
            text: qsTr("OK! Now registering your BitShares Account. Just a moment...")
         }
         PropertyChanges {
            target: wallet
            onErrorRegistering: {
               errorMessage = error
               state = "ERROR"
            }
         }
      },
      State {
         name: "ERROR"
         PropertyChanges {
            target: statusText
            text: errorMessage
            color: visuals.errorGlowColor
         }
      }
   ]
}
