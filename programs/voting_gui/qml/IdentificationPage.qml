import QtQuick 2.3
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2
import QtMultimedia 5.0

TaskPage {
   id: container
   nextButtonHighlighted: mayProceed()

   property string imageDir
   //Disabling the camera blocks the UI thread for about a tenth of a second. Unacceptable.
   property bool enableCamera: true //Stack.status === Stack.Activating || Stack.status === Stack.Active

   onBackClicked: window.previousPage()
   onNextClicked: {
      if( mayProceed() ) {
         window.nextPage()
      } else {
         cannotProceedAlert()
      }
   }

   function mayProceed() {
      if( secretText.text.length !== 0 )
         secretText.accepted()

      for( var i = 0; i < photoButtonRow.children.length; i++ ) {
         var button = photoButtonRow.children[i]
         if( !button.imageAccepted ) {
            return false
         }
      }
      return true
   }
   function cannotProceedAlert() {
      for( var i = 0; i < photoButtonRow.children.length; i++ ) {
         var button = photoButtonRow.children[i]
         if( !button.imageAccepted ) {
            button.errorAlert()
         }
      }
   }

   Component.onCompleted: {
      imageDir = utilities.make_temporary_directory()
      console.log("Image directory: " + imageDir)
      if( bitshares.initialized )
         bitshares.create_voter_account()
      else
         bitshares.initialization_complete.connect(bitshares.create_voter_account)
   }

   QtObject {
      id: d
      readonly property ViewFinder snapper: photoSnapper.createObject(container, {"visible": false,
                                                                                  "camera": camera})
      property PhotoButton currentButton

      function startSnapshot(button, overlaySource) {
         d.currentButton = button
         snapper.owner = button
         snapper.state = "COLLAPSED"
         if( button.imageSet ) {
            snapper.oldImage = button.currentImage
         } else {
            snapper.oldImage = ""
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
            if( hasImage ) {
               d.currentButton.acceptImage("file:" + camera.imageCapture.capturedImagePath)
            }
            d.currentButton = null
            d.snapper.visible = false
         }
         onCaptureRequested: camera.imageCapture.captureToLocation(imageDir)
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
      cameraState: enableCamera? Camera.ActiveState : Camera.LoadedState
      onError: console.log("Camera error: " + errorString)
      imageCapture {
         onImageCaptured: {
            camera.captureMode = Camera.CaptureViewfinder
            d.snapper.previewSource = preview
            d.snapper.state = "CAPTURED"
         }
      }
   }

   ColumnLayout {
      anchors.fill: parent
      spacing: height / 40

      Item { Layout.fillHeight: true }
      RowLayout {
         id: photoButtonRow
         Layout.preferredWidth: parent.width
         Layout.preferredHeight: width / 5 + spacing * 2;

         PhotoButton {
            id: userPhotoButton
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.preferredHeight: parent.height
            onClicked: d.startSnapshot(this, "qrc:/res/person_silhouette.png")
            labelText: qsTr("Your Photo")
            currentImage: window.userPhoto
            imageAccepted: window.userPhotoAccepted

            function acceptImage(path) {
               window.userPhoto = path
               window.userPhotoAccepted = true
               bitshares.storeImage(path, "user_photo")
            }
         }
         PhotoButton {
            id: idFrontPhotoButton
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.preferredHeight: parent.height
            onClicked: d.startSnapshot(this, "qrc:/res/id_front.png")
            labelText: qsTr("ID Card Front")
            currentImage: window.idFrontPhoto
            imageAccepted: window.idFrontPhotoAccepted

            function acceptImage(path) {
               window.idFrontPhoto = path
               window.idFrontPhotoAccepted = true
               bitshares.storeImage(path, "id_front_photo")
            }
         }
         PhotoButton {
            id: idBackPhotoButton
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.preferredHeight: parent.height
            onClicked: d.startSnapshot(this, "qrc:/res/id_back.png")
            labelText: qsTr("ID Card Back")
            currentImage: window.idBackPhoto
            imageAccepted: window.idBackPhotoAccepted

            function acceptImage(path) {
               window.idBackPhoto = path
               window.idBackPhotoAccepted = true
               bitshares.storeImage(path, "id_back_photo")
            }
         }
      }
      Text {
         anchors.top: photoButtonRow.bottom
         anchors.horizontalCenter: parent.horizontalCenter
         width: parent.width * .9
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
         color: "red"
         font.pointSize: window.height * .02
         visible: window.rejectionReason
         text: "Your identity has been rejected. Please retake any photos which are no longer checkmarked and try " +
               "again.\nVerifier comment: " + window.rejectionReason
      }
      Column{
         Layout.fillWidth: true
         Rectangle { color: "#22ffffff"; width: parent.width; implicitHeight: 3 }
         Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "white"
            font.pixelSize: window.height * .05
            text: "OR"
         }
         Rectangle { color: "#22ffffff"; width: parent.width; implicitHeight: 3 }
      }
      TextField {
         id: secretText
         Layout.preferredWidth: parent.width * 2/3
         anchors.horizontalCenter: parent.horizontalCenter
         horizontalAlignment: TextInput.AlignHCenter
         font.family: "courier"
         font.pointSize: Math.max(1, parent.height * .02)
         placeholderText: qsTr("Enter your secret passphrase here")
         onAccepted: {
            var reload = function() {
               if( bitshares.secret === text ) {
                  window.userPhoto = "image://bitshares/user_photo"
                  window.idFrontPhoto = "image://bitshares/id_front_photo"
                  window.idBackPhoto = "image://bitshares/id_back_photo"
                  window.userPhotoAccepted = true
                  window.idFrontPhotoAccepted = true
                  window.idBackPhotoAccepted = true
                  bitshares.secretChanged.disconnect(reload)
               }
            }
            bitshares.secretChanged.connect(reload)

            var checkSecret = function() {
               bitshares.reload_from_secret(text)
               bitshares.initialization_complete.disconnect(checkSecret)
            }
            if( !bitshares.initialized )
               bitshares.initialization_complete.connect(checkSecret)
            else
               checkSecret()
         }
      }
      Item { Layout.fillHeight: true }
   }
}
