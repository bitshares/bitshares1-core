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
   }
   property int checkpointsComplete: 0

   RowLayout {
      id: roadMapBar
      anchors.fill: parent
      spacing: 20

      Repeater {
         model: checkpoints
         delegate: Component {
            Item {
               anchors.verticalCenter: parent.verticalCenter
               height: parent.height
               Layout.fillWidth: true

               QtObject {
                  id: d
                  property bool completed: index < checkpointsComplete
                  property bool inProgress: index == checkpointsComplete
               }

               Image {
                  source: d.inProgress? "qrc:/res/halfchecked.png"
                                      : d.completed? "qrc:/res/checked.png"
                                                   : "qrc:/res/unchecked.png"
                  anchors.top: parent.top
                  anchors.bottom: checkpointLabel.top
                  anchors.horizontalCenter: parent.horizontalCenter
                  fillMode: Image.PreserveAspectFit
               }

               Label {
                  id: checkpointLabel
                  text: title
                  anchors.horizontalCenter: parent.horizontalCenter
                  anchors.bottom: parent.bottom
                  horizontalAlignment: Text.AlignHCenter
               }
            }
         }
      }

   }
}
