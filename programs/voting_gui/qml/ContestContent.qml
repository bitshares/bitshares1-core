import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Item {
   id: content

   property real fontSize: 12

   function breakLines(text) {
      return text.replace("\n", "<br/>")
   }
   function dropLineBreaks(text) {
      return text.replace("\n", " ")
   }

   Text {
      id: instructionText
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.leftMargin: contestContainer.radius / 2
      anchors.rightMargin: anchors.leftMargin
      color: "white"
      font.pointSize: fontSize * .75
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      text: {
         switch( tags["decision type"] ) {
         case "vote one":
            return "Choose a candidate below."
         case "vote many":
            return "Choose at most " + tags["vote many limit"] + " candidates below."
         case "vote yes/no":
            switch( tags["contest type"] ) {
            case "Judicial":
               return "Shall " + contestants.get(0).description + " "
                     + contestants.get(0).name + " be elected to the office for the term provided by law?"
            case "Measures Submitted To The Voters":
               return description + "<br/>" + dropLineBreaks(contestants.get(0).description)
                     + "<br/><br/>Do you wish to approve this measure?"
            }
         }
      }

      SequentialAnimation {
         id: errorAnimation
         PropertyAnimation { target: instructionText; property: "color"; from: "white"; to: "red"; duration: 200 }
         PropertyAnimation { target: instructionText; property: "color"; from: "red"; to: "white"; duration: 200 }
      }
   }

   Component {
      id: voteOneOrManyDecision
      VoteOneOrManyDecision {
         contestantList: contestants
         writeInSlots: "write-in slots" in tags? tags["write-in slots"] : 0
         winnerLimit: "vote many limit" in tags? tags["vote many limit"] : 1
         fontSize: content.fontSize
      }
   }
   Component {
      id: voteYesNoDecision
      VoteYesNoDecision {
         buttonHeight: content.fontSize * 3
      }
   }

   Loader {
      anchors.top: instructionText.bottom
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.margins: contestContainer.radius / 2
      sourceComponent: {
         switch( tags["decision type"] ) {
         case "vote many":
         case "vote one":
            return voteOneOrManyDecision
         case "vote yes/no":
            return voteYesNoDecision
         }
      }
   }
}
