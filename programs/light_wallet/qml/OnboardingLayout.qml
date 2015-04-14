import QtQuick 2.3
import QtQuick.Controls 1.2
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
         showMessage("Registration failed: " + reason)
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

   Timer {
      interval: 1
      running: true
      onTriggered: nameField.forceActiveFocus()
      repeat: false
   }

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
         value: 0
         opacity: 0

         NumberAnimation {
            id: registrationProgressAnimation
            target: registrationProgress
            property: "value"
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
            helperText: qsTr("This password will unlock your account only on this %1, so it can be short and easy to remember.").arg(deviceType())
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
                  showMessage("Unable to connect to server.", "Try Again", connectToServer)
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
         id: importBrainKeyField
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width - visuals.margins*2
         placeholderText: qsTr("Recovery Password")
         helperText: qsTr("This is the password you were asked to write down when you backed up your account.")
         floatingLabel: true
         onEditingFinished: importNameField.findRecoverableAccounts(text)
         onTextChanged: nameSearchTimer.restart()
         transform: ShakeAnimation { id: brainKeyShaker }

         Timer {
            id: nameSearchTimer
            interval: 500
            onTriggered: importNameField.findRecoverableAccounts(importBrainKeyField.text)
            repeat: false
         }
      }
      MenuField {
         id: importNameField
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width - visuals.margins*2
         helperText: (model && model.length === 0)? qsTr("Enter your recovery password above, then select an account to recover here.")
                                                  : qsTr("Select the account to recover.")
         transform: ShakeAnimation { id: importNameShaker }

         function findRecoverableAccounts(key) {
            if( onboarder.state !== "IMPORTING" ) return
            showMessage(qsTr("Searching for accounts to recover..."))
            importNameField.model = wallet.recoverableAccounts(key)
            if( importNameField.model.length === 0 )
               showMessage(qsTr("Could not find any accounts to recover. Is your recovery password correct?"))
         }
      }
      PasswordField {
         id: importPasswordField
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width - visuals.margins*2
         placeholderText: qsTr("Unlocking Password")
         helperText: qsTr("This password will unlock your account only on this %1, so it can be short and easy to remember.").arg(deviceType())
         onAccepted: finishImportButton.clicked()
         transform: ShakeAnimation { id: importPasswordShaker }
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
            onClicked: {
               if( !importNameField.model || importNameField.model.length === 0 )
               {
                  if( importBrainKeyField.activeFocus )
                     return importNameField.findRecoverableAccounts(importBrainKeyField.text)
                  return brainKeyShaker.shake()
               }
               if( !importNameField.selectedText )
                  return importNameShaker.shake()
               if( !importPasswordField.password )
                  return importPasswordShaker.shake()

               if( wallet.recoverWallet(importNameField.selectedText, importPasswordField.password, importBrainKeyField.text) )
                  onboarder.finished()
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
         PropertyChanges {
            target: registrationProgress
            opacity: 1
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
         ScriptAction {
            script: importBrainKeyField.forceActiveFocus()
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
         ScriptAction {
            script: nameField.forceActiveFocus()
         }
      }
   ]
}
