/**
  * Invoke slot when event fires, then disconnect. If predicate is callable, then slot will be invoked and disconnected
  * only if predicate returns true.
  */
function connectOnce(event, slot, predicate) {
   var task = function() {
      if( !( typeof(predicate) === "function" ) || predicate() ) {
         slot.apply(this, arguments)
         event.disconnect(task)
      }
   }
   event.connect(task)
}

function copyTextToClipboard(text) {
   //Hack to get clipboard access in QML
   var input = Qt.createQmlObject("import QtQuick 2.2; TextInput{visible: false}", window)
   input.text = text
   input.selectAll()
   input.copy()
   input.destroy()
}
