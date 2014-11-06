import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtMultimedia 5.0

Rectangle {
   id: container
   color: "#d9d9d9"

   QtObject {
      id: d
      property Item snapper
      property PhotoButton currentButton

      function startSnapshot(button) {
         d.currentButton = button
         d.snapper = photoSnapper.createObject(button, {"owner": button})

         if( d.snapper === null ) {
            console.log("Error instantiating photo snapper: " + snapper.errorString())
            return
         }

         d.snapper.state = "EXPANDED"
         camera.cameraState = Camera.ActiveState
      }
   }
   Component {
      id: photoSnapper

      ViewFinder {
         source: camera
         viaItem: overlord
         expandedContainer: container
         onExpanding: {
            if( d.currentButton.imageSet )
            {
               previewSource = d.currentButton.currentImage
               hasImage = true
            }
         }
         onExpanded: previewOpacity = 0
         onCollapsed: {
            d.currentButton.currentImage = "file:" + camera.imageCapture.capturedImagePath
            d.currentButton = null
            d.snapper.destroy()
         }
         onCaptureRequested: camera.imageCapture.capture()
      }
   }
   Item {
      id: overlord
      //This item exists to be the via property of the ParentAnimation of the ViewFinder as it collapses.
      //Without it, the ViewFinder sinks below the camera buttons while collapsing.
      z: 5
   }
   Camera {
      id: camera
      cameraState: Camera.LoadedState
      onError: console.log("Camera error: " + errorString)
      imageCapture {
         onImageCaptured: {
            d.snapper.previewSource = preview
            d.snapper.state = "CAPTURED"
            camera.cameraState = Camera.LoadedState
         }
      }
   }

   RowLayout {
      anchors {
         left: parent.left
         right: parent.right
         verticalCenter: parent.verticalCenter
         verticalCenterOffset: -height / 3
      }
      height: width / 5 + spacing * 2;

      PhotoButton {
         Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
         Layout.fillHeight: true
         onClicked: d.startSnapshot(this)
         labelText: qsTr("Your Photo")
      }
      PhotoButton {
         Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
         Layout.fillHeight: true
         onClicked: d.startSnapshot(this)
         labelText: qsTr("ID Card Front")
      }
      PhotoButton {
         Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
         Layout.fillHeight: true
         onClicked: d.startSnapshot(this)
         labelText: qsTr("ID Card Back")
      }
      PhotoButton {
         Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
         Layout.fillHeight: true
         onClicked: d.startSnapshot(this)
         labelText: qsTr("Voter Registration")
      }
   }
}
