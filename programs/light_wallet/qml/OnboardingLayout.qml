import QtQuick 2.3
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

import "utils.js" as Utils

MainView {
   id: onboarder

   property alias username: nameField.text
   property string errorMessage
   property string faucetUrl: "http://faucet.bitshares.org"

   signal finished

   function useFaucet() {
      return ["ios","android"].indexOf(Qt.platform.os) < 0 && AppName !== "lw_xts"
   }
   function registerAccount() {
      onboarder.state = "REGISTERING"
      Utils.connectOnce(wallet.accounts[username].isRegisteredChanged, finished,
                        function() { return wallet.accounts[username].isRegistered })
      Utils.connectOnce(wallet.onErrorRegistering, function(reason) {
         showError("Registration failed: " + reason)
         console.log("Can't register: " + reason)
      })

      if( !useFaucet() ) {
         if( wallet.connected ) {
            registrationProgressAnimation.start()
            wallet.registerAccount(username)
         } else {
            // Not connected. Schedule for when we reconnect.
            wallet.runWhenConnected(function() {
               registrationProgressAnimation.start()
               wallet.registerAccount(username)
            })
         }
      } else {
         var account = wallet.accounts[username]
         Qt.openUrlExternally(encodeURI(faucetUrl+"?owner_key="+account.ownerKey+"&active_key="+account.activeKey+
                                        "&account_name="+username+"&app_id="+persist.guid))
         wallet.pollForRegistration(username);
      }
   }
   function passwordEntered(password) {
      wallet.createWallet(username, password)
      registerAccount()
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
      id: baseLayoutColumn
      anchors.verticalCenter: parent.verticalCenter
      anchors.horizontalCenter: parent.horizontalCenter
      width: Math.min(parent.width - visuals.margins * 2, units.dp(600))
      spacing: visuals.margins

      Label {
         anchors.horizontalCenter: parent.horizontalCenter
         horizontalAlignment: Text.AlignHCenter
         text: qsTr("Welcome to %1").arg(Qt.application.organization)
         style: "headline"
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      Label {
         anchors.horizontalCenter: parent.horizontalCenter
         text: qsTr("Version %1").arg(Qt.application.version)
         color: Theme.light.hintColor
         style: "caption"
      }
      Label {
         id: statusText
         text: qsTr("To get started, create a password below.\n" +
                    "This password can be short and easy to remember — we'll make a better one later.")
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width
         height: units.dp(50)
         style: "subheading"
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
         verticalAlignment: Text.AlignVCenter
      }
      ProgressBar {
         id: registrationProgress
         anchors.horizontalCenter: parent.horizontalCenter
         width: units.dp(400)
         progress: 0

         NumberAnimation {
            id: registrationProgressAnimation
            target: registrationProgress
            property: "progress"
            from: 0; to: 1
            duration: 15000
            easing.type: Easing.OutQuart
            onRunningChanged: {
               if( !running ) {
                  registrationProgress.progress = 0
               }
            }
         }
      }
      ColumnLayout {
         width: parent.width

         TextField {
            id: nameField
            input.inputMethodHints: Qt.ImhLowercaseOnly | Qt.ImhLatinOnly
            input.font.pixelSize: units.dp(20)
            placeholderText: qsTr("Pick a Username")
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            text: wallet.accountNames.length? wallet.accountNames[0] : ""
            helperText: defaultHelperText
            characterLimit: 64
            onEditingFinished: if( wallet.connected ) nameAvailable()
            transform: ShakeAnimation { id: nameShaker }

            property string defaultHelperText: qsTr("May contain letters, numbers and hyphens.")

            function nameAvailable() {
               if( text.length === 0 ) {
                  helperText = defaultHelperText
                  hasError = false
               } else if( !wallet.isValidAccountName(text) || text.indexOf('.') >= 0 || wallet.accountExists(text) ) {
                  helperText = qsTr("That username is already taken")
                  hasError = true
               } else if( characterCount > characterLimit ) {
                  helperText = qsTr("Name is too long")
                  hasError = true
               } else {
                  helperText = defaultHelperText
                  hasError = false
               }

               return !hasError
            }
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
            text: qsTr("Register Account")
            Layout.fillWidth: true
            Layout.preferredHeight: passwordField.height

            onClicked: {
               if( !wallet.connected ) {
                  showError("Unable to connect to server.", "Try Again", connectToServer)
                  return
               }

               if( nameField.text.length === 0 || !nameField.nameAvailable() )
                  return nameShaker.shake()

               if( passwordField.password.length < 1 ) {
                  passwordField.shake()
               } else {
                  passwordEntered(passwordField.password)
               }
            }
         }
      }
      RowLayout {
         spacing: visuals.margins
         width: parent.width
         Rectangle {
            color: Theme.light.hintColor
            height: 1
            anchors.verticalCenter: parent.verticalCenter
            Layout.fillWidth: true
         }
         Label {
            text: qsTr("OR")
            style: "headline"
            color: Theme.light.hintColor
         }
         Rectangle {
            color: Theme.light.hintColor
            height: 1
            anchors.verticalCenter: parent.verticalCenter
            Layout.fillWidth: true
         }
      }
      Button {
         id: importButton
         width: parent.width
         text: qsTr("Import Account")
         onClicked: onboarder.state = "IMPORTING"
      }
   }

   Column {
      id: importLayout
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.top: parent.bottom
      width: Math.min(parent.width, units.dp(600))

      TextField {
         id: importNameField
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width - visuals.margins*2
         placeholderText: qsTr("Account Name")
         helperText: qsTr("Must be the same as when you originally created your account.")
         floatingLabel: true
      }
      TextField {
         id: importBrainKeyField
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width - visuals.margins*2
         placeholderText: qsTr("Recovery Password")
         helperText: qsTr("This is the password you were asked to write down when you backed up your account.")
         floatingLabel: true
      }
      PasswordField {
         id: importPasswordField
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width - visuals.margins*2
         placeholderText: qsTr("Unlocking Password")
         helperText: qsTr("This password can be short and easy to remember.")
         onAccepted: finishImportButton.clicked()
      }
      Item {
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width - visuals.margins*2
         height: childrenRect.height

         Button {
            anchors.right: finishImportButton.left
            anchors.rightMargin: units.dp(16)
            text: qsTr("Go Back")
            onClicked: onboarder.state = ""
         }
         Button {
            id: finishImportButton
            anchors.right: parent.right
            anchors.rightMargin: units.dp(16)
            text: qsTr("Import Account")
            onClicked: if( wallet.recoverWallet(importNameField.text, importPasswordField.password, importBrainKeyField.text) )
                          onboarder.finished()
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
            text: useFaucet()? qsTr("Please complete your registration in the browser window. Your wallet will open shortly after you finish.")
                             : qsTr("OK! Now registering your BitShares Account. Just a moment...")
         }
         PropertyChanges {
            target: wallet
            onErrorRegistering: {
               errorMessage = error
               state = "ERROR"
            }
         }
         PropertyChanges {
            target: importButton
            enabled: false
         }
      },
      State {
         name: "IMPORTING"
         AnchorChanges {
            target: baseLayoutColumn
            anchors.bottom: onboarder.top
            anchors.verticalCenter: undefined
         }
         AnchorChanges {
            target: importLayout
            anchors.top: undefined
            anchors.verticalCenter: onboarder.verticalCenter
         }
         PropertyChanges {
            target: importNameField
            focus: true
         }
      },
      State {
         name: "ERROR"
         PropertyChanges {
            target: statusText
            text: errorMessage
            color: "red"
         }
      }
   ]
   transitions: [
      Transition {
         from: ""
         to: "IMPORTING"
         AnchorAnimation {
            duration: 500
            easing.type: Easing.OutQuad
         }
         PropertyAnimation {
            duration: 200
            target: importLayout
            property: "opacity"
            from: 0; to: 1
         }
         PropertyAnimation {
            duration: 200
            target: baseLayoutColumn
            property: "opacity"
            from: 1; to: 0
         }
      },
      Transition {
         from: "IMPORTING"
         to: ""
         AnchorAnimation {
            duration: 500
            easing.type: Easing.OutQuad
         }
         PropertyAnimation {
            duration: 200
            target: importLayout
            property: "opacity"
            from: 1; to: 0
         }
         PropertyAnimation {
            duration: 200
            target: baseLayoutColumn
            property: "opacity"
            from: 0; to: 1
         }
      }
   ]
}
