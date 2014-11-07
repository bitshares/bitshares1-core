import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import QtMultimedia 5.0

Button {
   width: height
   implicitWidth: width
   iconSource: "qrc:/res/camera.png"

   property alias labelText: label.text
   property alias currentImage: image.source
   property bool imageSet: image.status !== Image.Null

   Text {
      id: label
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      anchors.bottomMargin: parent.height / 10
      font.pointSize: 16
      visible: !imageSet
   }

   Image {
      id: image
      anchors.fill: parent
      anchors.margins: 3
      fillMode: Image.PreserveAspectFit
      mirror: true
   }
}
