import QtQuick 2.3
import QtQuick.Controls 1.2
import QtGraphicalEffects 1.0

Rectangle {
   id: button
   radius: height / 4

   property alias text: label.text
   property alias textWidth: label.width
   property bool checkable: false
   property bool checked: false
   property bool highlighted: checkable && checked
   property bool pulsing: false
   property ExclusiveGroup exclusiveGroup

   QtObject {
      id: d
      property ExclusiveGroup oldGroup
   }

   onExclusiveGroupChanged: {
      if( exclusiveGroup ) {
         checkable = true
         exclusiveGroup.bindCheckable(button)
      } else {
         checkable = false
      }

      if( d.oldGroup )
         d.oldGroup.unbindCheckable(button)
      d.oldGroup = exclusiveGroup
   }

   signal clicked
   onClicked: if( checkable ) checked = !checked

   RectangularGlow {
      id: highlighter
      anchors.fill: parent
      anchors.margins: parent.height / 4
      cornerRadius: parent.radius
      color: parent.color
      glowRadius: parent.height
      visible: parent.highlighted

      SequentialAnimation {
         running: button.pulsing
         loops: Animation.Infinite
         PropertyAnimation {
            target: highlighter
            property: "opacity"
            duration: 500
            from: 0; to: 1
            easing.type: "OutQuad"
         }
         PropertyAnimation {
            target: highlighter
            property: "opacity"
            duration: 500
            from: 1; to: 0
            easing.type: "InQuad"
         }
      }
   }
   Image {
      anchors.fill: parent
      fillMode: Image.PreserveAspectFit
      horizontalAlignment: Image.AlignLeft
      verticalAlignment: Image.AlignTop
      source: "qrc:/res/checked.png"
      opacity: checked? .7 : 0
      Behavior on opacity { NumberAnimation {} }
   }
   Text {
      id: label
      anchors.centerIn: parent
      font.pointSize: Math.max(parent.height * .8, 1) / (lineCount * 1.3)
      color: "white"
   }

   MouseArea {
      anchors.fill: parent
      enabled: parent.visible
      onClicked: parent.clicked()
   }
}
