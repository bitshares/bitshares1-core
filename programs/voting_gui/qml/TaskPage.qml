import QtQuick 2.3
import QtGraphicalEffects 1.0

Item {
   signal backClicked
   signal nextClicked

   property alias backButtonVisible: backButton.visible
   property bool backButtonHighlighted: false
   property alias nextButtonVisible: nextButton.visible
   property bool nextButtonHighlighted: false

   RectangularGlow {
      id: backGlow
      anchors.fill: backButton
      anchors.margins: backButton.height / 4
      cornerRadius: parent.radius
      color: backButton.color
      glowRadius: backButton.height
      visible: backButtonHighlighted

      SequentialAnimation {
         running: backButtonHighlighted
         loops: Animation.Infinite
         PropertyAnimation {
            target: backGlow
            property: "opacity"
            duration: 500
            from: 0; to: 1
            easing.type: "OutQuad"
         }
         PropertyAnimation {
            target: backGlow
            property: "opacity"
            duration: 500
            from: 1; to: 0
            easing.type: "InQuad"
         }
      }
   }
   SimpleButton {
      id: backButton
      color: "red"
      anchors {
         bottom: parent.bottom
         left: parent.left
         margins: 40
      }
      height: parent.height * .05
      width: height * 4
      text: qsTr("Back")
      onClicked: backClicked()
   }
   RectangularGlow {
      id: nextGlow
      anchors.fill: nextButton
      anchors.margins: nextButton.height / 4
      cornerRadius: parent.radius
      color: nextButton.color
      glowRadius: nextButton.height
      visible: nextButtonHighlighted

      SequentialAnimation {
         running: nextButtonHighlighted
         loops: Animation.Infinite
         PropertyAnimation {
            target: nextGlow
            property: "opacity"
            duration: 500
            from: 0; to: 1
            easing.type: "OutQuad"
         }
         PropertyAnimation {
            target: nextGlow
            property: "opacity"
            duration: 500
            from: 1; to: 0
            easing.type: "InQuad"
         }
      }
   }
   SimpleButton {
      id: nextButton
      color: "green"
      anchors {
         bottom: parent.bottom
         right: parent.right
         margins: 40
      }
      height: parent.height * .05
      width: height * 4
      text: qsTr("Next")
      onClicked: nextClicked()
   }
}
