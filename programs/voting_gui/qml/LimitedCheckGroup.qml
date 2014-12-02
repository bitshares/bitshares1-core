import QtQuick 2.3
import QtQuick.Controls 1.2

ExclusiveGroup {
   property int checkLimit: 1
   property int checkedCount: 0
   property var __boundCheckables: ({})

   signal checkBlocked

   function bindCheckable(object) {
      var handler = function() {
         if( object.checked && checkedCount >= checkLimit ) {
            //Increment checkedCount, but immediately uncheck, which calls this handler again and decrements
            checkedCount++
            object.checked = false
            checkBlocked()
            return
         }
         
         if( object.checked )
            checkedCount++
         else
            checkedCount--
      }
      object.checkedChanged.connect(handler)
      __boundCheckables[object] = handler
      if( object.checked )
         checkedCount++
   }
   function unbindCheckable(object) {
      if( object in __boundCheckables ) {
         object.checkedChanged.disconnect(__boundCheckables[object])
         delete __boundCheckables[object]
         if( object.checked )
            checkedCount--
      }
   }
}
