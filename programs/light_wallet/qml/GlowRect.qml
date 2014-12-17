import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0

RectangularGlow {
   id: glowBox
   opacity: 0
   visible: opacity > 0
   glowRadius: 5
   spread: 0.2
   z: -1

   function pulse() {
      pulser.restart()
   }
   
   SequentialAnimation {
      id: pulser
      PropertyAnimation {
         target: glowBox
         property: "opacity"
         from: 0; to: 1
         duration: 200
      }
      PropertyAnimation {
         target: glowBox
         property: "opacity"
         from: 1; to: 0
         duration: 200
      }
   }
}
