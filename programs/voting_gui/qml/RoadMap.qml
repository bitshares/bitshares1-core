import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Item {
   property ListModel checkpoints: ListModel {
      ListElement {
         title: "Identification"
      }
      ListElement {
         title: "Verification"
      }
      ListElement {
         title: "Registration"
      }
      ListElement {
         title: "Voting"
      }
      ListElement {
         title: "Finished"
      }
   }
   property int checkpointsComplete: 0
   readonly property variant currentCheckpoint: checkpoints.get(checkpointsComplete)

   RowLayout {
      id: roadMapBar
      anchors.fill: parent
      spacing: 20

      Repeater {
         model: checkpoints
         delegate: CheckPoint {
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            Layout.fillWidth: true
         }
      }
   }
}
