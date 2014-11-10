import QtQuick 2.3
import QtQuick.Layouts 1.1
import QtMultimedia 5.0

Rectangle {
   id: viewFinder
   z: greyer.z + 1

   property Item owner
   property Item viaItem
   property Item expandedContainer
   property Camera camera
   property url oldImage
   readonly property GreySheet greyer: greyOut.createObject(expandedContainer)
   property alias viewFinderOverlaySource: overlay.source
   property alias previewSource: photoPreview.source
   property bool hasImage: false

   signal expanding
   signal expanded
   signal captureRequested
   signal collapsing
   signal collapsed

   onOwnerChanged: {
      parent = owner
      width = owner.width
      height = width / 16 * 9
      x = 0
      y = owner.height / 2 - height / 2
   }

   Component {
      id: greyOut
      GreySheet {}
   }

   states: [
      State {
         name: "EXPANDED"
         ParentChange {
            target: viewFinder
            parent: expandedContainer
            width: expandedContainer.width * .75
            height: width / 16 * 9
            x: expandedContainer.width / 2 - width / 2
            y: expandedContainer.height / 2 - height / 2
         }
         PropertyChanges {
            target: photoPreview
            source: oldImage
            opacity: 0
         }
         PropertyChanges {
            target: videoOutput
            visible: true
         }
         PropertyChanges {
            target: captureButton
            visible: true
         }
         PropertyChanges {
            target: greyer
            state: "GREYED"
            onClicked: {
               viewFinder.state = "COLLAPSED"
               hasImage = false
            }
         }
      },
      State {
         name: "COUNTDOWN"
         extend: "EXPANDED"
         PropertyChanges {
            target: captureButton
            visible: false
         }
         PropertyChanges {
            target: greyer
            state: "GREYED"
            onClicked: {}
         }
      },
      State {
         name: "CAPTURED"
         extend: "EXPANDED"
         PropertyChanges {
            target: photoPreview
            opacity: 1
            source: "file:" + camera.imageCapture.capturedImagePath
            restoreEntryValues: false
         }
         PropertyChanges {
            target: videoOutput
            visible: false
         }
         PropertyChanges {
            target: captureButton
            visible: false
         }
         PropertyChanges {
            target: retakeAcceptButtons
            visible: true
         }
      },
      State {
         name: "COLLAPSED"
         PropertyChanges {
            target: photoPreview
            opacity: 1
            source: hasImage? "file:" + camera.imageCapture.capturedImagePath
                            : oldImage
         }
         PropertyChanges {
            target: videoOutput
            visible: false
         }
         ParentChange {
            target: viewFinder
            parent: owner
            width: owner.width
            height: width / 16 * 9
            x: 0
            y: owner.height / 2 - height / 2
         }
         PropertyChanges {
            target: greyer
            state: "CLEAR"
            onClicked: {}
         }
      }
   ]
   transitions: [
      Transition {
         to: "EXPANDED"
         ParentAnimation {
            via: viaItem
            SequentialAnimation {
               ScriptAction { script: viewFinder.expanding() }
               NumberAnimation { target: viewFinder; properties: "x,y,width,height"; duration: 400; easing.type: "InOutQuad" }
               ScriptAction { script: viewFinder.expanded() }
            }
         }
      },
      Transition {
         to: "COUNTDOWN"
         SequentialAnimation {
            ScriptAction { script: { camera.captureMode = Camera.CaptureStillImage } }
            PropertyAction { target: countdownText; property: "text"; value: "3" }
            PropertyAnimation { target: countdownText; property: "opacity"; from: 0; to: 1 }
//            PauseAnimation { duration: 400 }
            PropertyAnimation { target: countdownText; property: "opacity"; from: 1; to: 0 }
            PropertyAction { target: countdownText; property: "text"; value: "2" }
            PropertyAnimation { target: countdownText; property: "opacity"; from: 0; to: 1 }
//            PauseAnimation { duration: 400 }
            PropertyAnimation { target: countdownText; property: "opacity"; from: 1; to: 0 }
            PropertyAction { target: countdownText; property: "text"; value: "1" }
            PropertyAnimation { target: countdownText; property: "opacity"; from: 0; to: 1 }
//            PauseAnimation { duration: 400 }
            PropertyAnimation { target: countdownText; property: "opacity"; from: 1; to: 0 }
            ScriptAction { script: { captureRequested() } }
         }
      },
      Transition {
         from: "EXPANDED,CAPTURED"
         to: "COLLAPSED"
         ParentAnimation {
            via: viaItem
            SequentialAnimation {
               ScriptAction { script: viewFinder.collapsing() }
               NumberAnimation { target: viewFinder; properties: "x,y,width,height"; duration: 300; easing.type: "InQuad" }
               ScriptAction { script: viewFinder.collapsed() }
            }
         }
      }
   ]

   MouseArea {
      anchors.fill: parent
   }
   Image {
      id: placeholder
      anchors.fill: parent
      fillMode: Image.Pad
      source: "qrc:/res/camera.png"
      horizontalAlignment: Image.AlignHCenter
      verticalAlignment: Image.AlignVCenter
      mirror: true
   }
   VideoOutput {
      id: videoOutput
      anchors.fill: parent
      transform: Rotation {
         origin {x: videoOutput.width / 2; y: 0}
         axis {x: 0; y: 1; z: 0}
         angle: 180
      }
      source: camera

      Image {
         id: overlay
         anchors.fill: parent
         fillMode: Image.PreserveAspectCrop
         horizontalAlignment: Image.AlignHCenter
         verticalAlignment: Image.AlignBottom
         visible: status !== Image.Null
      }
   }
   Text {
      id: countdownText
      anchors.centerIn: parent
      font.pointSize: Math.max(parent.height / 3, 1)
      color: "white"
      style: Text.Outline
      styleColor: "black"
      opacity: 0
   }
   Image {
      id: photoPreview
      fillMode: Image.Stretch
      horizontalAlignment: Image.AlignHCenter
      verticalAlignment: Image.AlignVCenter
      anchors.fill: parent

      Behavior on opacity { NumberAnimation { duration: 400 } }
   }
   SimpleButton {
      id: captureButton
      color: "green"
      anchors {
         bottom: parent.bottom
         left: parent.left
         right: parent.right
         margins: 20
      }
      height: width / 10
      visible: false
      text: qsTr("Capture")
      onClicked: viewFinder.state = "COUNTDOWN"
   }
   RowLayout {
      id: retakeAcceptButtons
      anchors {
         bottom: parent.bottom
         left: parent.left
         right: parent.right
         margins: 20
      }
      height: width / 10
      spacing: 20
      visible: false

      SimpleButton {
         id: retakeButton
         color: "yellow"
         Layout.fillWidth: true
         Layout.fillHeight: true
         text: qsTr("Retake")
         onClicked: viewFinder.state = "EXPANDED"
      }
      SimpleButton {
         id: acceptButton
         color: "green"
         Layout.fillWidth: true
         Layout.fillHeight: true
         text: qsTr("Accept")
         onClicked: {
            hasImage = true
            viewFinder.state = "COLLAPSED"
         }
      }
   }
}
