import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Rectangle {
   id: contestContainer
   radius: width / 50
   border.width: 2
   border.color: "grey"
   color: "#aa000000"
   height: expanded? expandedHeight : collapsedHeight
   clip: true

   property bool expanded: false
   property alias decision: content.decision
   property real fontSize
   readonly property real collapsedHeight: header.height
   readonly property real expandedHeight: content.y + content.height

   signal expansionComplete
   signal collapseComplete

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
      fontSize: contestContainer.fontSize
   }

   Behavior on height {
      SequentialAnimation {
         NumberAnimation { easing.type: "InOutQuint" }
         ScriptAction { script: {
               if( expanded ) contestContainer.expansionComplete()
               else contestContainer.collapseComplete()
            }
         }
      }
   }
}
