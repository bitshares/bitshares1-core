import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0

import Material 0.1

RowLayout {
   id: passwordForm
   transform: Translate {
      id: passwordTransform
      x: 0
   }

   property alias placeholderText: passwordText.placeholderText
   property alias password: passwordText.text
   property alias fontPixelSize: passwordText.font.pixelSize

   onFocusChanged: if( focus ) passwordText.focus = true

   function shake() {
      errorShake.restart()
   }

   signal accepted

   SequentialAnimation {
      id: errorShake
      loops: 2
      alwaysRunToEnd: true

      PropertyAnimation {
         target: passwordTransform
         property: "x"
         from: 0; to: units.dp(10)
         easing.type: Easing.InQuad
         duration: 25
      }
      PropertyAnimation {
         target: passwordTransform
         property: "x"
         from: units.dp(10); to: units.dp(-10)
         easing.type: Easing.OutInQuad
         duration: 50
      }
      PropertyAnimation {
         target: passwordTransform
         property: "x"
         from: units.dp(-10); to: 0
         easing.type: Easing.OutQuad
         duration: 25
      }
   }

   TextField {
      id: passwordText
      Layout.fillWidth: true
      echoMode: button.pressed? TextInput.Normal : TextInput.Password
      inputMethodHints: Qt.ImhSensitiveData | Qt.ImhHiddenText
      readOnly: button.pressed
      floatingLabel: true

      onAccepted: passwordForm.accepted()

      SequentialAnimation {
         id: scrollTextAnimation
         loops: Animation.Infinite
         running: button.pressed

         NumberAnimation {
            target: passwordText
            property: "cursorPosition"
            from: 0; to: passwordText.text.length
            easing.type: Easing.OutQuad
            duration: Math.max((passwordText.text.length - 30) * 200, 200)
         }
         NumberAnimation {
            target: passwordText
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
