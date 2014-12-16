import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import org.BitShares.Types 1.0

ButtonStyle {
   background: Rectangle {
      color: control.hovered?
                (control.pressed?
                    visuals.buttonPressedColor : visuals.buttonHoverColor) : visuals.buttonColor
   }
   label: Label {
      color: visuals.buttonTextColor
      font.pointSize: visuals.textBaseSize
      text: control.text
      horizontalAlignment: Text.AlignHCenter
      verticalAlignment: Text.AlignVCenter
   }
}
