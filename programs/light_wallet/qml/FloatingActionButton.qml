import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

import Material 0.1

import "utils.js" as Utils

Item {
   anchors.bottom: parent.bottom
   anchors.right: parent.right
   anchors.margins: Device.type === Device.phone? units.dp(16) : units.dp(24)
   width: units.dp(80)
   height: width
   
   property alias iconName: actionIcon.name
   property alias iconColor: actionIcon.color
   property alias color: visibleActionButton.backgroundColor
   
   signal triggered
   
   View {
      id: visibleActionButton
      anchors.centerIn: parent
      radius: width / 2
      width: units.dp(56)
      height: width
      elevation: 2
      backgroundColor: Theme.primaryColor
      
      Icon {
         id: actionIcon
         anchors.centerIn: parent
         color: "white"
      }
   }
   Ink {
      anchors.fill: parent
      onClicked: parent.triggered()
      endSize: visibleActionButton.width - 10
   }
}
