import QtQuick 2.3
import QtQuick.Controls 1.2

ApplicationWindow {
   visible: true
   width: 1280
   height: 1024
   title: qsTr("Voting Booth")

   Component {
      id: taskPage
      Rectangle {
         color: "#d9d9d9"

         property variant currentCheckpoint

         Text {
            anchors.centerIn: parent
            text: parent.currentCheckpoint.title
            font.pointSize: 40
         }
      }
   }

   RoadMap {
      id: roadMap
      anchors {
         top: parent.top
         left: parent.left
         right: parent.right
      }
      height: Math.max(45, parent.height / 20)

      checkpoints: ListModel {
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

      MouseArea {
         anchors.fill: parent;
         onClicked: {
            if( parent.checkpointsComplete < parent.checkpoints.count-1 ) {
               parent.checkpointsComplete++
               votingUiStack.push({item: taskPage,
                                     properties: {currentCheckpoint: roadMap.currentCheckpoint},
                                     replace: true})
            } else {
               parent.checkpointsComplete = 0
               votingUiStack.clear()
               votingUiStack.push({"item": taskPage, "properties": {"currentCheckpoint": roadMap.currentCheckpoint}, "immediate": true})
            }
         }
      }
   }
   StackView {
      id: votingUiStack
      anchors {
         top: roadMap.bottom
         left: parent.left
         right: parent.right
         bottom: parent.bottom
      }
      initialItem: {"item": taskPage, "properties": {"currentCheckpoint": roadMap.currentCheckpoint}}
      delegate: StackViewDelegate {
         function transitionFinished(properties)
         {
            properties.exitItem.scale = 1
            properties.exitItem.color = "#d9d9d9"
         }

         pushTransition: StackViewTransition {
            PropertyAnimation {
               target: enterItem
               property: "x"
               from: enterItem.width
               to: 0
               easing.type: "InQuad"
            }
            PropertyAnimation {
               target: enterItem
               property: "opacity"
               from: 0.5
               to: 1
               easing.type: "InQuad"
            }
            PropertyAnimation {
               target: exitItem
               property: "color"
               from: "#d9d9d9"
               to: Qt.darker("#d9d9d9")
            }
            PropertyAnimation {
               target: exitItem
               property: "scale"
               from: 1
               to: .8
            }
         }
      }

      Rectangle {
         anchors.fill: parent
         color: "black"
         z: -1
      }
   }
}
