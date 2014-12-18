import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

ColumnLayout {
   spacing: visuals.spacing

   property bool firstTime: true

   signal passwordEntered(string password)
   
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
      anchors.horizontalCenter: parent.horizontalCenter
      Layout.fillWidth: true
      text: qsTr("To get started, create a password below.\n" +
                 "This password can be short and easy to remember -- we'll make a better one later.")
      color: visuals.lightTextColor
      font.pixelSize: visuals.textBaseSize
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere

      Component.onCompleted: visible = firstTime
   }
   ColumnLayout {
      Layout.fillWidth: true
      PasswordField {
         id: passwordField
         Layout.fillWidth: true
         placeholderText: firstTime?
                             qsTr("Create a Password") : qsTr("Enter Password")
         onAccepted: openButton.clicked()
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
}
