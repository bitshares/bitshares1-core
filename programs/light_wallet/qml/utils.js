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
