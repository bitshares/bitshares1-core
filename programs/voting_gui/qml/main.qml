import QtQuick 2.3
import QtQuick.Controls 1.2

ApplicationWindow {
   id: window
   visible: true
   width: 1280
   height: 1024
   title: qsTr("Voting Booth")

   property url userPhoto
   property url idFrontPhoto
   property url idBackPhoto
   property url voterRegistrationPhoto

   //Called by individual pages via window.nextPage()
   function nextPage() {
      roadMap.checkpointsComplete++
      votingUiStack.push({"item": Qt.resolvedUrl(roadMap.currentCheckpoint.page)})
   }

   Connections {
      target: bitshares
      onError: console.log("Error from backend: " + errorString)
   }
   Image {
      anchors.fill: parent
      fillMode: Image.Tile
      source: "qrc:/res/bg.png"
   }
   Image {
      anchors.centerIn: parent
      height: parent.height / 2
      source: "qrc:/res/logo.png"
      fillMode: Image.PreserveAspectFit
      smooth: true
      opacity: .1
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
            page: "IdentificationPage.qml"
         }
         ListElement {
            title: "Verification"
            page: "VerificationPage.qml"
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
               votingUiStack.push({"item": Qt.resolvedUrl(roadMap.currentCheckpoint.page)})
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
      initialItem: IdentificationPage {
         enableCamera: Stack.status === Stack.Activating || Stack.status === Stack.Active
      }
      delegate: StackViewDelegate {
         function transitionFinished(properties)
         {
            properties.exitItem.scale = 1
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
               property: "scale"
               from: 1
               to: .8
            }
         }
      }

      Component {
         id: taskPage
         TaskPage {
            property variant currentCheckpoint

            Text {
               anchors.centerIn: parent
               text: parent.currentCheckpoint.title
               color: "white"
               font.pointSize: 40
            }
         }
      }
   }
}
