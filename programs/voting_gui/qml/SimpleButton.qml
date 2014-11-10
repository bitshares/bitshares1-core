import QtQuick 2.3

Rectangle {
   radius: height / 4
   opacity: .4

   property alias text: label.text

   signal clicked
   
   Text {
      id: label
      anchors.centerIn: parent
      font.pointSize: Math.max(parent.height * .8, 1)
      color: "white"
   }
   MouseArea {
      anchors.fill: parent
      enabled: parent.visible
      onClicked: parent.clicked()
   }
}
