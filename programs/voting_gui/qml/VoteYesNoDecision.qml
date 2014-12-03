import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

RowLayout {
   property real buttonHeight: 30

   function getDecision() {
      var decision = {"voter_opinions": []}

      if( yesNoButtonsGroup.current === null || !yesNoButtonsGroup.current.checked )
         //Neither a yes vote nor a no vote. Create an invalid decision.
         decision = {}
      else
         decision["voter_opinions"] = [[0, (yesNoButtonsGroup.current.text === "Yes")*1]]

      return decision
   }
   function setDecision(decision) {
      if( "voter_opinions" in decision && decision["voter_opinions"] instanceof Array
            && decision["voter_opinions"].length === 1 && decision["voter_opinions"][0] instanceof Array
            && decision["voter_opinions"][0].length === 2 ) {
         if( decision["voter_opinions"][0][1] !== 1 )
            yesButton.checked = true
         else
            noButton.checked = true
      }
   }

   ExclusiveGroup { id: yesNoButtonsGroup }
   SimpleButton {
      id: noButton
      exclusiveGroup: yesNoButtonsGroup
      opacity: 1
      color: "#88ff0000"
      Layout.fillWidth: true
      Layout.minimumWidth: textWidth
      Layout.preferredWidth: parent.width / 2
      text: "No"
      height: buttonHeight
   }
   SimpleButton {
      id: yesButton
      exclusiveGroup: yesNoButtonsGroup
      opacity: 1
      color: "#8800ff00"
      Layout.fillWidth: true
      Layout.minimumWidth: textWidth
      Layout.preferredWidth: parent.width / 2
      text: "Yes"
      height: buttonHeight
   }
}
