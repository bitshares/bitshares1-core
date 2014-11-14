import QtQuick 2.3

Item {
   signal nextClicked

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
