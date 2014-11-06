import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import QtMultimedia 5.0

Button {
   width: height
   implicitWidth: width

   property alias currentImage: image.source
   property bool imageSet: image.status !== Image.Null

   Image {
      id: image
      anchors.fill: parent
      anchors.margins: 3
      fillMode: Image.PreserveAspectFit
   }
}
