import QtQuick 2.3

Rectangle {
   id: sheet
   anchors.fill: parent
   z: 100
   color: "black"
   opacity: 0
   visible: opacity > 0

   signal clicked
   
   states: [
      State {
         name: "GREYED"
         PropertyChanges {
            target: sheet
            opacity: .8
         }
         PropertyChanges {
            target: mousetrap
            enabled: true
         }
      },
      State {
         name: "CLEAR"
         PropertyChanges {
            target: sheet
            opacity: 0
         }
         PropertyChanges {
            target: mousetrap
            enabled: false
         }
      }
   ]
   transitions: [
      Transition { PropertyAnimation { target: sheet; property: "opacity"; duration: 300; easing.type: "InQuad" } }
   ]

   MouseArea {
      id: mousetrap
      anchors.fill: sheet
      onClicked: sheet.clicked()
   }
}
