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

   Column {
      id: layout
      anchors.centerIn: parent
      spacing: units.dp(8)
      width: parent.width - visuals.margins * 2

      Label {
         anchors.horizontalCenter: parent.horizontalCenter
         horizontalAlignment: Text.AlignHCenter
         text: qsTr("Welcome back")
         style: "headline"
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      PasswordField {
         id: passwordField
         width: parent.width
         placeholderText: qsTr("Enter Password")
         onAccepted: openButton.clicked()
         fontPixelSize: units.dp(20)
         focus: true

         Connections {
            target: wallet
            onErrorUnlocking: passwordField.shake()
         }
      }
      Button {
         id: openButton
         text: qsTr("Open")
         width: passwordField.width
         onClicked: {
            if( passwordField.password.length < 1 ) {
               passwordField.shake()
            } else {
               passwordEntered(passwordField.password)
            }
         }
      }
   }
}
