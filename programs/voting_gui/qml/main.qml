import QtQuick 2.3
import QtQuick.Controls 1.2

ApplicationWindow {
   visible: true
   width: 1280
   height: 1024
   title: qsTr("Voting Booth")

   RoadMap {
      anchors {
         top: parent.top
         left: parent.left
         right: parent.right
      }
      height: Math.max(45, parent.height / 20)

      MouseArea {
         anchors.fill: parent;
         onClicked: parent.checkpointsComplete = (parent.checkpointsComplete + 1) % parent.checkpoints.count
      }
   }
}
