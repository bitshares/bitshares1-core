import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Item {
   QtObject {
      id: d
      property bool completed: index < checkpointsComplete || checkpointsComplete == checkpoints.count - 1
      property bool inProgress: index == checkpointsComplete
   }
   
   Image {
      source: d.completed? "qrc:/res/checked.png"
                         : d.inProgress? "qrc:/res/halfchecked.png"
                                       : "qrc:/res/unchecked.png"
      anchors.top: parent.top
      anchors.bottom: checkpointLabel.top
      anchors.horizontalCenter: parent.horizontalCenter
      fillMode: Image.PreserveAspectFit
   }
   
   Label {
      id: checkpointLabel
      text: (index + 1) + ". " + title
      color: "white"
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      horizontalAlignment: Text.AlignHCenter
   }
}
