import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import "utils.js" as Utils

Item {
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

   ColumnLayout {
      id: layout
      anchors.fill: parent
      anchors.margins: visuals.margins
      spacing: visuals.spacing

      Item {
         Layout.preferredHeight: window.orientation === Qt.PortraitOrientation?
                                    window.height / 4 : window.height / 6
      }
      Label {
         anchors.horizontalCenter: parent.horizontalCenter
         horizontalAlignment: Text.AlignHCenter
         text: qsTr("Welcome to BitShares")
         color: visuals.textColor
         font.pixelSize: visuals.textBaseSize * 1.5
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      Label {
         id: statusText
         text: qsTr("To get started, create a password below.\n" +
                    "This password can be short and easy to remember — we'll make a better one later.")
         anchors.horizontalCenter: parent.horizontalCenter
         Layout.fillWidth: true
         color: visuals.lightTextColor
         font.pixelSize: visuals.textBaseSize
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      ColumnLayout {
         Layout.fillWidth: true

         TextField {
            id: nameField
            inputMethodHints: Qt.ImhLowercaseOnly
            placeholderText: qsTr("Pick a Username")
            font.pixelSize: visuals.textBaseSize * 1.1
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            text: wallet.account? wallet.account.name : ""
         }
         PasswordField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: qsTr("Create a Password")
            fontPixelSize: visuals.textBaseSize * 1.1
            onAccepted: openButton.clicked()

            Connections {
               target: wallet
               onErrorUnlocking: passwordField.errorGlow()
            }
         }
         Button {
            id: openButton
            style: WalletButtonStyle {}
            text: qsTr("Begin")
            Layout.fillWidth: true
            Layout.preferredHeight: passwordField.height

            onClicked: {
               if( wallet.account )
                  wallet.account.name = nameField.text

               if( passwordField.password.length < 1 ) {
                  passwordField.errorGlow()
               } else {
                  passwordEntered(passwordField.password)
               }
            }
         }
      }
      Item { Layout.fillHeight: true }
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
