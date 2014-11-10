import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import QtMultimedia 5.0

Rectangle {
   radius: height / 4
   opacity: .4

   property alias text: label.text

   signal clicked
   
   Text {
      id: label
      anchors.centerIn: parent
      font.pointSize: parent.height - 20
      color: "white"
   }
   MouseArea {
      anchors.fill: parent
      enabled: parent.visible
      onClicked: parent.clicked()
   }
}
