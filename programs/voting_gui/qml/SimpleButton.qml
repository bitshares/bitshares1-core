import QtQuick 2.3
import QtQuick.Controls 1.2

Rectangle {
   id: button
   radius: height / 4
   opacity: .4
   implicitWidth: label.width

   property alias text: label.text
   property bool checkable: false
   property bool checked: false
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
   
   Text {
      id: label
      anchors.centerIn: parent
      font.pointSize: Math.max(parent.height * .8, 1) / (lineCount * 1.3)
      color: "white"
   }
   Image {
      anchors.fill: parent
      fillMode: Image.PreserveAspectFit
      horizontalAlignment: Image.AlignLeft
      verticalAlignment: Image.AlignTop
      source: "qrc:/res/checked.png"
      opacity: checked? .3 : 0
      Behavior on opacity { NumberAnimation {} }
   }

   MouseArea {
      anchors.fill: parent
      enabled: parent.visible
      onClicked: parent.clicked()
   }
}
