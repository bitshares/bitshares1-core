import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
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
            target: mouseArea
            onClicked: {
               hasImage = true
               viewFinder.captureRequested()
            }
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
            target: greyer
            state: "GREYED"
            onClicked: {
               viewFinder.state = "COLLAPSED"
               hasImage = false
            }
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
            target: mouseArea
            onClicked: { viewFinder.state = "COLLAPSED" }
         }
      },
      State {
         name: "COLLAPSED"
         PropertyChanges {
            target: photoPreview
            opacity: 1
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
               ScriptAction {
                  script: {
                     viewFinder.expanded()
                  }
               }
            }
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
               ScriptAction {
                  script: {
                     viewFinder.collapsed()
                  }
               }
            }
         }
      }
   ]

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
   Image {
      id: photoPreview
      fillMode: Image.Stretch
      horizontalAlignment: Image.AlignHCenter
      verticalAlignment: Image.AlignVCenter
      anchors.fill: parent
      mirror: true

      Behavior on opacity { NumberAnimation { duration: 400 } }
   }
   MouseArea {
      id: mouseArea
      anchors.fill: parent
   }
}
