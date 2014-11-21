import QtQuick 2.3

Image {
   id: spinner
   fillMode: Image.PreserveAspectFit
   source: "qrc:/res/v.png"
   smooth: true

   property bool running: false
   onRunningChanged: animator.running = running

   Image {
      id: ring
      anchors.fill: parent
      fillMode: Image.PreserveAspectFit
      source: "qrc:/res/ring.png"
      smooth: true
      RotationAnimator {
         id: animator
         target: ring
         from: 0; to: 450
         duration: 2000
         alwaysRunToEnd: true
         easing.type: "InOutQuad"
         onStopped: {
            if( !spinner.running ) {
               return
            }

            from = ring.rotation % 360
            to = from + 450
            start()
         }
      }
   }
}
