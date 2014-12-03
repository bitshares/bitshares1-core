import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Rectangle {
   id: contestContainer
   radius: width / 50
   border.width: 2
   border.color: "grey"
   color: "#aa000000"
   height: expanded? content.y + content.height : header.height
   clip: true

   property bool expanded: false
   property alias fontSize: content.fontSize
   property alias decision: content.decision

   function setDecision(decision) {
      if( typeof decision !== "undefined" )
         content.setDecision(decision)
   }

   ContestHeader {
      id: header
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.leftMargin: contestContainer.radius
      anchors.rightMargin: anchors.leftMargin
      fontSize: contestContainer.fontSize
      leftText: tags["name"]

      onClicked: contestContainer.expanded = !contestContainer.expanded
   }
   ContestContent {
      id: content
      anchors.top: header.bottom
      height: childrenRect.y + childrenRect.height + contestContainer.radius
      width: parent.width
   }

   Behavior on height { NumberAnimation { easing.type: "InOutQuint" } }
}
