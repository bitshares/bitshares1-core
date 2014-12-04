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
   property var decisions: ({})

   QtObject {
      id: d

      property var poppedItems: []
   }

   //Called by individual pages via window.nextPage() and window.previousPage()
   function nextPage() {
      if( d.poppedItems.length === 0 ) {
         roadMap.checkpointsComplete++
         votingUiStack.push({"item": Qt.resolvedUrl(roadMap.currentCheckpoint.page), "destroyOnPop": false})
      } else {
         votingUiStack.push(d.poppedItems.pop())
      }
   }
   function previousPage() {
      if( votingUiStack.depth <= 1 )
         return;
      d.poppedItems.push(votingUiStack.pop())
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
            title: "Welcome"
            page: "WelcomePage.qml"
         }
         ListElement {
            title: "Identification"
            page: "IdentificationPage.qml"
         }
         ListElement {
            title: "Registration"
            page: "RegistrationPage.qml"
         }
         ListElement {
            title: "Vote"
            page: "VotePage.qml"
         }
         ListElement {
            title: "Cast"
            page: "CastPage.qml"
         }
         ListElement {
            title: "Confirmation"
            page: "ConfirmationPage.qml"
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
      initialItem: WelcomePage{}
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
               from: 0
               to: 1
               easing.type: "InQuad"
            }
            PropertyAnimation {
               target: exitItem
               property: "opacity"
               from: 1
               to: 0
               easing.type: "InQuad"
            }
            PropertyAnimation {
               target: exitItem
               property: "scale"
               from: 1
               to: .8
            }
         }
         popTransition: StackViewTransition {
            PropertyAnimation {
               target: exitItem
               property: "x"
               from: 0
               to: exitItem.width
               easing.type: "InQuad"
            }
            PropertyAnimation {
               target: exitItem
               property: "opacity"
               from: 1
               to: 0
               easing.type: "InQuad"
            }
            PropertyAnimation {
               target: enterItem
               property: "opacity"
               from: 0
               to: 1
               easing.type: "InQuad"
            }
            PropertyAnimation {
               target: enterItem
               property: "scale"
               from: .8
               to: 1
            }
         }
      }
   }
}
