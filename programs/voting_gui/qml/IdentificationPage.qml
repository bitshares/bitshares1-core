import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtMultimedia 5.0

Rectangle {
   id: container
   color: "#d9d9d9"

   QtObject {
      id: d
      readonly property ViewFinder snapper: photoSnapper.createObject(this, {"visible": false,
                                                                             "camera": camera})
      property PhotoButton currentButton

      function startSnapshot(button, overlaySource) {
         d.currentButton = button
         snapper.owner = button
         snapper.state = "COLLAPSED"
         if( button.imageSet ) {
            snapper.oldImage = button.currentImage
         } else {
            snapper.oldImage = button.iconSource
         }
         snapper.visible = true

         if( typeof overlaySource !== "undefined" ) {
            d.snapper.viewFinderOverlaySource = overlaySource
         } else {
            d.snapper.viewFinderOverlaySource = ""
         }

         d.snapper.state = "EXPANDED"
      }
   }
   Component {
      id: photoSnapper

      ViewFinder {
         viaItem: overlord
         expandedContainer: container
         onExpanding: {
            if( d.currentButton.imageSet )
            {
               previewSource = d.currentButton.currentImage
               hasImage = true
            }
         }
         onCollapsed: {
            if( hasImage )
               d.currentButton.currentImage = "file:" + camera.imageCapture.capturedImagePath
            d.currentButton = null
            d.snapper.visible = false
         }
         onCaptureRequested: camera.imageCapture.capture()
      }
   }
   Item {
      id: overlord
      //This item exists to be the via property of the ParentAnimation of the ViewFinder as it collapses.
      //Without it, the ViewFinder sinks below the camera buttons while collapsing.
      z: d.snapper.greyer.z + 1
   }
   Camera {
      id: camera
      cameraState: container.Stack.status === Stack.Activating || container.Stack.status === Stack.Active?
                      Camera.ActiveState : Camera.LoadedState
      onError: console.log("Camera error: " + errorString)
      imageCapture {
         onImageCaptured: {
            d.snapper.previewSource = preview
            d.snapper.state = "CAPTURED"
         }
      }
   }

   RowLayout {
      id: photoButtonRow
      anchors {
         left: parent.left
         right: parent.right
         verticalCenter: parent.verticalCenter
         verticalCenterOffset: -height / 3
      }
      height: width / 5 + spacing * 2;

      PhotoButton {
         id: firstPhotoButton
         Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
         Layout.fillHeight: true
         onClicked: d.startSnapshot(this, "qrc:/res/person_silhouette.png")
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
