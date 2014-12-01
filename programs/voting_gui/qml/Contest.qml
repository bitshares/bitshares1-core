import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

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
   ContestContent {
      id: content
      anchors.top: header.bottom
      height: childrenRect.y + childrenRect.height + contestContainer.radius
      width: parent.width
   }

   Behavior on height { NumberAnimation { easing.type: "InOutQuint" } }
}
