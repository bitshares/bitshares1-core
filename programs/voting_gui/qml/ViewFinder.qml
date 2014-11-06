import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import QtMultimedia 5.0

Rectangle {
   id: viewFinder
   clip: true
   width: owner.width
   height: width / 16 * 9
   x: 0
   y: owner.height / 2 - height / 2

   property Item owner
   property Item viaItem
   property Item expandedContainer
   property alias previewVisible: photoPreview.opacity
   property alias previewSource: photoPreview.source
   property alias source: videoOutput.source

   signal expanding
   signal expanded
   signal captureRequested
   signal collapsing
   signal collapsed

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
            onClicked: viewFinder.captureRequested()
         }
         PropertyChanges {
            target: promptText
            text: qsTr("Click to take photo")
         }
      },
      State {
         name: "CAPTURED"
         extend: "EXPANDED"
         PropertyChanges {
            target: photoPreview
            opacity: 1
            fillMode: Image.Stretch
         }
         PropertyChanges {
            target: mouseArea
            onClicked: { viewFinder.state = "COLLAPSED" }
         }
         PropertyChanges {
            target: videoOutput
            visible: false
         }
         PropertyChanges {
            target: promptText
            text: qsTr("Click to close")
         }
      },
      State {
         name: "COLLAPSED"
         PropertyChanges {
            target: promptText
            visible: false
         }
         PropertyChanges {
            target: photoPreview
            opacity: 1
            fillMode: Image.Stretch
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
            target: videoOutput
            visible: false
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
               NumberAnimation { target: viewFinder; properties: "x,y,width,height"; duration: 1000; easing.type: "OutQuad" }
               ScriptAction {
                  script: {
                     promptText.visible = true
                     viewFinder.expanded()
                  }
               }
            }
         }
      },
      Transition {
         to: "COLLAPSED"
         ParentAnimation {
            via: viaItem
            SequentialAnimation {
               ScriptAction { script: viewFinder.collapsing() }
               NumberAnimation { target: viewFinder; properties: "x,y,width,height"; duration: 1000; easing.type: "InQuad" }
               ScriptAction { script: viewFinder.collapsed() }
            }
         }
      }
   ]

   VideoOutput {
      id: videoOutput
      anchors.fill: parent
   }
   Image {
      id: photoPreview
      source: "qrc:/res/camera.png"
      fillMode: Image.Pad
      horizontalAlignment: Image.AlignHCenter
      verticalAlignment: Image.AlignVCenter
      anchors.fill: parent

      Behavior on opacity { NumberAnimation { duration: 400 } }

      Rectangle {
         anchors.fill: parent
         z: -1
      }
   }
   MouseArea {
      id: mouseArea
      anchors.fill: parent
   }
   Text {
      id: promptText
      anchors.horizontalCenter: parent.horizontalCenter
      anchors.bottom: parent.bottom
      anchors.bottomMargin: height * 2
      font.pointSize: 30
      visible: false
      opacity: .4
      style: Text.Outline
      styleColor: "grey"
   }
}
