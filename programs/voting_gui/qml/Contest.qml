import QtQuick 2.3
import QtQuick.Controls 1.2

Rectangle {
   id: contestContainer
   radius: width / 50
   border.width: 2
   border.color: "grey"
   color: "#aa000000"
   height: expanded? childrenRect.height : header.height
   clip: true

   property bool expanded: false
   property real fontSize

   Item {
      id: header
      width: parent.width
      anchors.top: parent.top
      height: headerText.y + headerText.height + 10

      Text {
         id: headerText
         color: "white"
         anchors.left: parent.left
         anchors.right: parent.right
         anchors.top: parent.top
         anchors.leftMargin: contestContainer.radius
         anchors.rightMargin: anchors.leftMargin
         anchors.topMargin: height / 2
         font.pointSize: fontSize
         text: tags["name"]
         elide: Text.ElideRight
      }
      MouseArea {
         anchors.fill: parent
         hoverEnabled: false
         onClicked: contestContainer.expanded = !contestContainer.expanded
      }
   }
   Item {
      id: content
      anchors.top: header.bottom
      anchors.topMargin: 20
      height: header.height * 3
   }

   Behavior on height { NumberAnimation { easing.type: "InOutQuint" } }
}
