import QtQuick 2.3
import QtQuick.Layouts 1.1

Image {
   fillMode: Image.PreserveAspectFit
   source: "qrc:/res/v.png"
   smooth: true

   property alias running: animator.running
   
   Image {
      id: ring
      anchors.fill: parent
      fillMode: Image.PreserveAspectFit
      source: "qrc:/res/ring.png"
      smooth: true
      RotationAnimator {
         id: animator
         target: ring
         from: 0; to: 360
         loops: Animation.Infinite
         running: true
         duration: 1500
      }
   }
}
