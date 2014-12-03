import QtQuick 2.3
import QtGraphicalEffects 1.0

Item {
   signal backClicked
   signal nextClicked

   property alias backButtonVisible: backButton.visible
   property bool backButtonHighlighted: false
   property alias nextButtonVisible: nextButton.visible
   property bool nextButtonHighlighted: false
   property real buttonAreaHeight: height - (backButton.y - backButton.anchors.margins)

   SimpleButton {
      id: backButton
      color: "#88ff0000"
      anchors {
         bottom: parent.bottom
         left: parent.left
         margins: 40
      }
      height: parent.height * .05
      width: height * 4
      highlighted: backButtonHighlighted
      pulsing: backButtonHighlighted
      text: qsTr("Back")
      onClicked: backClicked()
   }
   SimpleButton {
      id: nextButton
      color: "#8800ff00"
      anchors {
         bottom: parent.bottom
         right: parent.right
         margins: 40
      }
      height: parent.height * .05
      width: height * 4
      highlighted: nextButtonHighlighted
      pulsing: nextButtonHighlighted
      text: qsTr("Next")
      onClicked: nextClicked()
   }
}
