import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0

import Material 0.1

RowLayout {
   id: passwordForm
   transform: ShakeAnimation {
      id: shaker
   }

   property alias placeholderText: passwordText.placeholderText
   property alias floatingLabel: passwordText.floatingLabel
   property alias helperText: passwordText.helperText
   property alias password: passwordText.text
   property alias fontPixelSize: passwordInput.font.pixelSize

   onFocusChanged: if( focus ) passwordText.focus = true

   function shake() {
      shaker.shake()
   }

   signal accepted

   TextField {
      id: passwordText
      Layout.fillWidth: true
      Layout.preferredHeight: implicitHeight
      floatingLabel: true
      input {
         id: passwordInput
         color: "black"
         echoMode: button.pressed? TextInput.Normal : TextInput.Password
         inputMethodHints: Qt.ImhSensitiveData | Qt.ImhHiddenText
         readOnly: button.pressed
      }

      onAccepted: passwordForm.accepted()

      SequentialAnimation {
         id: scrollTextAnimation
         loops: Animation.Infinite
         running: button.pressed

         NumberAnimation {
            target: passwordText.input
            property: "cursorPosition"
            from: 0; to: passwordText.text.length
            easing.type: Easing.OutQuad
            duration: Math.max((passwordText.text.length - 30) * 200, 200)
         }
         NumberAnimation {
            target: passwordText.input
            property: "cursorPosition"
            to: 0; from: passwordText.text.length
            easing.type: Easing.OutQuad
            duration: Math.max((passwordText.text.length - 30) * 200, 200)
         }
      }
   }
   Button {
      id: button
      height: units.dp(48)
      Layout.preferredHeight: passwordText.height / 1.7

      Icon {
         name: "image/remove_red_eye"
         anchors.centerIn: parent
         size: units.dp(32)
      }
   }
}
