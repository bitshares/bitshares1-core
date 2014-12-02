import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Item {
   id: content

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
      id: voteOneDecision
      GridLayout {
         columns: 4

         ExclusiveGroup { id: contestantButtonsGroup }
         Repeater {
            model: contestants
            delegate: SimpleButton {
               Layout.fillWidth: true
               Layout.fillHeight: true
               Layout.minimumWidth: implicitWidth
               exclusiveGroup: contestantButtonsGroup
               text: "<b>" + name + "</b><br/><br/>" + breakLines(description)
               height: fontSize * 4.5
            }
         }
         Component.onCompleted: {
            if( "write-in slots" in tags )
               console.log("Unsupported write-in on a vote one....")
         }
      }
   }
   Component {
      id: voteYesNoDecision
      RowLayout {

         ExclusiveGroup { id: yesNoButtonsGroup }
         SimpleButton {
            exclusiveGroup: yesNoButtonsGroup
            color: "red"
            Layout.fillWidth: true
            Layout.minimumWidth: implicitWidth
            text: "No"
            height: fontSize * 1.5
         }
         SimpleButton {
            exclusiveGroup: yesNoButtonsGroup
            color: "green"
            Layout.fillWidth: true
            Layout.minimumWidth: implicitWidth
            text: "Yes"
            height: fontSize * 1.5
         }
         Component.onCompleted: {
            if( "write-in slots" in tags )
               console.log("Unsupported write-in on a vote yes/no....")
         }
      }
   }
   Component {
      id: voteManyDecision
      GridLayout {
         columns: 4

         LimitedCheckGroup {
            id: limitedCheckGroup
            checkLimit: tags["vote many limit"]
            onCheckBlocked: errorAnimation.restart()
         }
         Repeater {
            model: contestants
            delegate: SimpleButton {
               Layout.fillWidth: true
               Layout.fillHeight: true
               Layout.minimumWidth: implicitWidth
               exclusiveGroup: limitedCheckGroup
               text: "<b>" + name + "</b><br/><br/>" + breakLines(description)
               height: fontSize * 4.5
            }
         }
         Component.onCompleted: {
            if( "write-in slots" in tags )
               console.log("Unsupported write-in on a vote one....")
         }
      }
   }

   Loader {
      anchors.top: instructionText.bottom
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.margins: contestContainer.radius / 2
      sourceComponent: {
         switch( tags["decision type"] ) {
         case "vote one":
            return voteOneDecision
         case "vote many":
            return voteManyDecision
         case "vote yes/no":
            return voteYesNoDecision
         }
      }
   }
}
