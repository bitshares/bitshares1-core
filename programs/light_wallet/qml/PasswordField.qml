import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import org.BitShares.Types 1.0

RowLayout {
   property alias placeholderText: passwordText.placeholderText
   property alias password: passwordText.text

   TextField {
      id: passwordText
      Layout.fillWidth: true
      font.pixelSize: visuals.textBaseSize * 1.1
      echoMode: button.pressed? TextInput.Normal : TextInput.Password
   }
   Button {
      id: button
      text: qsTr("Show")
      Layout.preferredHeight: passwordText.height
      style: WalletButtonStyle {}
   }
}
