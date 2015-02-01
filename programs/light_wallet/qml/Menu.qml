import QtQuick 2.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import Material 0.1

Card {
   id: floatingMenu
   flat: false
   width: units.dp(80)
   height: units.dp(Math.min(menuRepeater.model.length, 5) * 40 + 30)
   z: 10
   opacity: 0
   enabled: opened

   property alias model: menuRepeater.model
   property bool opened: false
   property Item owner

   /**
     Open the menu with the selectedElement positioned at (ownerX, ownerY)
     */
   function open(selectedElement, ownerX, ownerY) {
      var element = menuRepeater.itemAt(model.indexOf(selectedElement))
      var elementXY = element.mapToItem(owner, element.labelX, element.labelY)
      x += ownerX - elementXY.x
      y += ownerY - elementXY.y
      opened = true
   }
   function close() {
      opened = false
   }

   signal elementSelected(var elementData)
   
   Column {
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.verticalCenter: parent.verticalCenter
      anchors.margins: units.dp(15)
      Repeater {
         id: menuRepeater
         delegate: Item {
            height: units.dp(45)
            width: parent.width

            property alias labelX: label.x
            property alias labelY: label.y

            Label {
               id: label
               anchors.verticalCenter: parent.verticalCenter
               text: modelData
               font.pixelSize: units.dp(20)
               style: "menu"
            }
            Ink {
               anchors.fill: parent
               onClicked: {
                  floatingMenu.elementSelected(modelData)
                  floatingMenu.close()
               }
            }
         }
      }
   }

   states: [
      State {
         name: "opened"
         when: floatingMenu.opened
         PropertyChanges {
            target: floatingMenu
            opacity: 1
         }
      }
   ]

   transitions: [
      Transition {
         from: "*"
         to: "opened"
         reversible: true

         PropertyAnimation {
            target: floatingMenu
            property: "opacity"
         }
      }
   ]
}
