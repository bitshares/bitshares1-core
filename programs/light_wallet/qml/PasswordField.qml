import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0

RowLayout {
   id: passwordForm
   property alias placeholderText: passwordText.placeholderText
   property alias password: passwordText.text

   function errorGlow() {
      errorGlow.pulse()
   }

   signal accepted

   TextField {
      id: passwordText
      Layout.fillWidth: true
      font.pixelSize: visuals.textBaseSize * 1.1
      echoMode: button.pressed? TextInput.Normal : TextInput.Password
      inputMethodHints: Qt.ImhSensitiveData | Qt.ImhHiddenText
      readOnly: button.pressed

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
      GlowRect {
         id: errorGlow
         anchors.fill: passwordText
         color: visuals.errorGlowColor
      }
   }
   Button {
      id: button
      text: qsTr("Show")
      Layout.preferredHeight: passwordText.height
      style: WalletButtonStyle {}
   }
}
