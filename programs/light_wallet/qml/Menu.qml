import QtQuick 2.4
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
      var elementXY = menuRepeater.mapToItem(owner, element.x, element.y)
      console.log("(" + ownerX + "," + ownerY + ") -> (" + elementXY.x + "," + elementXY.y + ")")
      x += ownerX - elementXY.x
      y += ownerY - elementXY.y + element.height
      opened = true
   }
   function close() {
      opened = false
   }

   signal elementSelected(var elementData)
   
   Column {
      anchors.centerIn: parent
      anchors.margins: units.dp(15)
      spacing: units.dp(20)
      Repeater {
         id: menuRepeater
         delegate: Label {
            text: modelData
            font.pixelSize: units.dp(20)
            style: "menu"
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
