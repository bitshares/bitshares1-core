import QtQuick 2.3
import QtQuick.Controls 1.2
import QtGraphicalEffects 1.0

Button {
   width: height
   implicitWidth: width
   iconSource: imageSet? "" : "qrc:/res/camera.png"

   property alias labelText: label.text
   property alias currentImage: image.source
   property bool imageSet: image.status !== Image.Null

   function errorAlert() {
      errorAnimation.restart()
   }

   Text {
      id: label
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      anchors.bottomMargin: parent.height / 10
      font.pointSize: 16
   }

   Image {
      id: image
      anchors.fill: parent
      anchors.margins: 3
      fillMode: Image.PreserveAspectFit
   }

   RectangularGlow {
      id: redGlow
      anchors.fill: parent
      glowRadius: 0
      spread: 0.2
      color: "red"
      z: -1
   }

   SequentialAnimation {
      id: errorAnimation
      PropertyAnimation {
         target: redGlow
         property: "glowRadius"
         from: 0; to: 20
         duration: 200
      }
      PropertyAnimation {
         target: redGlow
         property: "glowRadius"
         from: 20; to: 0
         duration: 200
      }
   }
}
