import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

Item {
   property real minimumWidth: layout.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: layout.Layout.minimumHeight + visuals.margins * 2
   property alias username: nameField.text

   signal passwordEntered(string password)

   function clearPassword() {
      passwordField.password = ""
   }

   Stack.onStatusChanged: if( Stack.status === Stack.Active ) { nameField.focus = true }

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
            readOnly: true
         }
         PasswordField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: qsTr("Enter Password")
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
            text: qsTr("Open")
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
   }
}
