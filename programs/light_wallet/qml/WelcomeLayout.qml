import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Page {
   showBackButton: false
   property real minimumWidth: layout.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: layout.Layout.minimumHeight + visuals.margins * 2

   signal passwordEntered(string password)

   function clearPassword() {
      passwordField.password = ""
   }

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
         text: qsTr("Welcome back, ") + wallet.account.name
         color: visuals.textColor
         font.pixelSize: visuals.textBaseSize * 1.5
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      ColumnLayout {
         Layout.fillWidth: true

         PasswordField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: qsTr("Enter Password")
            fontPixelSize: visuals.textBaseSize * 1.1
            onAccepted: openButton.clicked()
            focus: true

            Connections {
               target: wallet
               onErrorUnlocking: passwordField.shake()
            }
         }
         Button {
            id: openButton
            text: qsTr("Open")
            Layout.fillWidth: true
            Layout.preferredHeight: passwordField.height
            elevation: 1

            onClicked: {
               if( passwordField.password.length < 1 ) {
                  passwordField.shake()
               } else {
                  passwordEntered(passwordField.password)
               }
            }
         }
      }
      Item { Layout.fillHeight: true }
   }
}
