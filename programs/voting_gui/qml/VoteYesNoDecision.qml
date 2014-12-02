import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

RowLayout {
   property real buttonHeight: 30

   ExclusiveGroup { id: yesNoButtonsGroup }
   SimpleButton {
      exclusiveGroup: yesNoButtonsGroup
      color: "red"
      Layout.fillWidth: true
      Layout.minimumWidth: textWidth
      Layout.preferredWidth: parent.width / 2
      text: "No"
      height: buttonHeight
   }
   SimpleButton {
      exclusiveGroup: yesNoButtonsGroup
      color: "green"
      Layout.fillWidth: true
      Layout.minimumWidth: textWidth
      Layout.preferredWidth: parent.width / 2
      text: "Yes"
      height: buttonHeight
   }
}
