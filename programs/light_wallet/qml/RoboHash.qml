import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import Material 0.1

Column {
   property alias name: robotName.text
   property alias preferredWidth: robotName.implicitWidth

   Image {
      id: roboHash
      height: units.dp(64)
      width: units.dp(64)
      source: "image://robohash/" + robotName.text
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
      Layout.fillWidth: true
      Layout.maximumWidth: implicitWidth
      font.pixelSize: units.dp(16)
      elide: Text.ElideRight
      width: Math.min(parent.width, implicitWidth)
   }
}
