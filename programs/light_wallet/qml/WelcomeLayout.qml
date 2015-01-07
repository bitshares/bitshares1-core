import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

Item {
   property real minimumWidth: layout.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: layout.Layout.minimumHeight + visuals.margins * 2
   property bool firstTime: true
   property alias username: nameField.text

   signal passwordEntered(string password)

   function clearPassword() {
      passwordField.password = ""
   }

   ColumnLayout {
      id: layout
      anchors.fill: parent
      anchors.margins: visuals.margins
      spacing: visuals.spacing

      Component.onCompleted: if( !firstTime ) passwordField.focus = true; else nameField.focus = true

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

         Component.onCompleted: visible = firstTime
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
            text: wallet.accountName
            readOnly: !firstTime
         }
         PasswordField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: firstTime?
                                qsTr("Create a Password") : qsTr("Enter Password")
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
            text: firstTime? qsTr("Begin") : qsTr("Open")
            Layout.fillWidth: true
            Layout.preferredHeight: passwordField.height

            onClicked: {
               if( passwordField.password.length < 1 ) {
                  passwordField.errorGlow()
               } else {
                  passwordEntered(passwordField.password)
               }
            }
         }
      }
      Item { Layout.fillHeight: true }

      states: [
         State {
            name: "REGISTERING"
            PropertyChanges {
               target: openButton
               enabled: false
            }
            PropertyChanges {
               target: statusText
               text: qsTr("OK! I'm creating your wallet. Just a sec...")
            }
            PropertyChanges {
               target: wallet
               onErrorRegistering: statusText.text = error
            }
         }
      ]
   }
}
