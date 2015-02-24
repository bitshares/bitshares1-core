import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

import Material 0.1

Column {
   property alias name: robotName.text
   property alias elideMode: robotName.elide
   spacing: units.dp(5)

   function __imageSource() {
      if( name.toLowerCase() === name )
         return "https://robohash.org/" + name + "?set=set1&size=200x200"
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
      smooth: true

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
      width: Math.min(implicitWidth, parent.width)
      anchors.horizontalCenter: parent.horizontalCenter
   }
}
