import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

import Material 0.1

import "jdenticon/jdenticon-1.0.1.min.js" as J

Column {
   property alias name: identiconName.text
   property alias elideMode: identiconName.elide
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
         if( name && name !== "Unknown" )
            J.jdenticon.drawIcon(canvas.getContext("2d"), wallet.sha256(name), canvas.height)
         else {
            var context = canvas.getContext("2d")
            context.reset()
            var draw_circle = function(context, x, y, radius) {
               context.beginPath()
               context.arc(x, y, radius, 0, 2 * Math.PI, false)
               context.fillStyle = "rgba(0, 0, 0, 0.1)"
               context.fill()
            }
            var size = canvas.height
            var centerX = size / 2
            var centerY = size / 2
            var radius = size/15
            draw_circle(context, centerX, centerY, radius)
            draw_circle(context, 2*radius, 2*radius, radius)
            draw_circle(context, centerX, 2*radius, radius)
            draw_circle(context, size - 2*radius, 2*radius, radius)
            draw_circle(context, size - 2*radius, centerY, radius)
            draw_circle(context, size - 2*radius, size - 2*radius, radius)
            draw_circle(context, centerX, size - 2*radius, radius)
            draw_circle(context, 2*radius, size - 2*radius, radius)
            draw_circle(context, 2*radius, centerY, radius)
         }
      }
   }
   Label {
      id: identiconName
      font.pixelSize: units.dp(16)
      elide: Text.ElideRight
      width: Math.min(implicitWidth, parent.width)
      anchors.horizontalCenter: parent.horizontalCenter
   }
}
