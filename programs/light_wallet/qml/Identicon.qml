import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

import Material 0.1

import "jdenticon/jdenticon-1.0.1.min.js" as J

Column {
   property alias name: robotName.text
   property alias elideMode: robotName.elide
   spacing: units.dp(5)

   onNameChanged: canvas.requestPaint()

   function __imageSource() {
      if( name.toLowerCase() === name )
         return "https://robohash.org/" + name + "?set=set1&size=200x200"
      else
         return "../res/bitshares.png"
   }

   Canvas {
      id: canvas
      anchors.horizontalCenter: parent.horizontalCenter
      height: units.dp(64)
      width: units.dp(64)
      contextType: "2d"

      onPaint: {
         J.jdenticon.drawIcon(canvas.getContext("2d"), wallet.sha256(name), canvas.height)
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
