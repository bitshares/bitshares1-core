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
   Item {
      id: content
      anchors.top: header.bottom
      height: childrenRect.y + childrenRect.height + contestContainer.radius

      Text {
         id: instructionText
         anchors.top: parent.top
         anchors.left: parent.left
         anchors.right: parent.right
         anchors.leftMargin: contestContainer.radius / 2
         anchors.rightMargin: anchors.leftMargin
         color: "white"
         font.pointSize: fontSize * .75
         text: "Choose a candidate:"
      }
      GridLayout {
         anchors.top: instructionText.bottom
         anchors.left: parent.left
         anchors.right: parent.right
         anchors.margins: contestContainer.radius / 2
         columns: 2

         ExclusiveGroup { id: contestantButtonsGroup }
         Repeater {
            model: contestants
            delegate: Button {
               exclusiveGroup: contestantButtonsGroup
               checkable: true
               checked: false
               text: "<b>" + name + "</b><br/><br/>" + breakLines(description)

               function breakLines(text) {
                  return text.replace("\n", "<br/>")
               }
            }
         }
      }
   }

   Behavior on height { NumberAnimation { easing.type: "InOutQuint" } }
}
