import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Column {
   property alias name: robotName.text
   property real preferredWidth: Math.max(robotName.implicitWidth, roboHash.width)

   function __imageSource() {
      if( name.toLowerCase() === name )
         return "https://robohash.org/" + name + "?size=" + Math.floor(roboHash.width) + "x" + Math.floor(roboHash.height)
      else
         return "../res/bitshares.png"
   }

   Image {
      id: roboHash
      anchors.horizontalCenter: parent.horizontalCenter
      height: units.dp(64)
      width: units.dp(64)
      source: __imageSource()
      fillMode: Image.PreserveAspectFit
      sourceSize {
         width: units.dp(64)
         height: units.dp(64)
      }

      AnimatedImage {
         anchors.fill: parent
         fillMode: Image.Pad
         visible: parent.status === Image.Loading
         source: "../res/spinner.gif"
      }
   }
   Label {
      id: robotName
      font.pixelSize: units.dp(16)
      elide: Text.ElideRight
      width: Math.min(parent.width, implicitWidth)
      horizontalAlignment: Text.AlignHCenter
      anchors.horizontalCenter: roboHash.horizontalCenter
   }
}
