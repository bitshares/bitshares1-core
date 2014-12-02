import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

GridLayout {
   columns: 4

   property var contestantList: []
   property int writeInSlots: 0
   property int winnerLimit: 1
   property real fontSize
   
   Component {
      id: voteOneGroup
      ExclusiveGroup { id: contestantButtonsGroup }
   }
   Component {
      id: voteManyGroup
      LimitedCheckGroup {
         id: limitedCheckGroup
         checkLimit: winnerLimit
         onCheckBlocked: errorAnimation.restart()
      }
   }
   Loader {
      id: contestantButtonsGroup
      sourceComponent: winnerLimit > 1?
                          voteManyGroup : voteOneGroup
   }
   Repeater {
      model: contestantList
      delegate: SimpleButton {
         Layout.fillWidth: true
         Layout.fillHeight: true
         Layout.minimumWidth: textWidth
         Layout.preferredWidth: parent.width / parent.columns
         exclusiveGroup: contestantButtonsGroup.item
         text: "<b>" + name + "</b><br/><br/>" + breakLines(description)
         height: fontSize * 4.5
      }
   }
   Component.onCompleted: {
      if( writeInSlots > 0 )
         console.log("Unsupported write-in on a vote one/many....")
   }
}
