import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0

import Material 0.1

Translate {
   id: passwordTransform
   x: 0

   function shake() {
      shaker.restart()
   }

   property SequentialAnimation shaker: SequentialAnimation {
      loops: 2
      alwaysRunToEnd: true
      
      PropertyAnimation {
         target: passwordTransform
         property: "x"
         from: 0; to: units.dp(10)
         easing.type: Easing.InQuad
         duration: 25
      }
      PropertyAnimation {
         target: passwordTransform
         property: "x"
         from: units.dp(10); to: units.dp(-10)
         easing.type: Easing.OutInQuad
         duration: 50
      }
      PropertyAnimation {
         target: passwordTransform
         property: "x"
         from: units.dp(-10); to: 0
         easing.type: Easing.OutQuad
         duration: 25
      }
   }
}
