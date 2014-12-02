import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import QtQuick.Dialogs 1.2

GridLayout {
   columns: 4

   property ListModel contestantList
   property int writeInSlots: 0
   property int winnerLimit: 1
   property real fontSize

   function getDecision() {
      var decision = {"voter_opinions": [], "write_in_names": []}
      if( winnerLimit === 1 ) {
         //Vote one decision; check the ExclusiveGroup.
         if( contestantButtonsGroup.item.current !== null && contestantButtonsGroup.item.current.checked ) {
            if( "contestantIndex" in contestantButtonsGroup.item.current ) {
               decision["voter_opinions"] = [[contestantButtonsGroup.item.current.contestantIndex, 1]]
            } else {
               decision["write_in_names"] = [contestantButtonsGroup.item.current.contestantName]
               decision["voter_opinions"] = [[contestantList.count, 1]]
            }
         } else decision = {}
      } else {
         //Vote many decision; iterate all contestants
         var writeInCount = 0;

         for( var i = 0; i < contestantRepeater.count; i++ ) {
            if( contestantRepeater.itemAt(i).checked )
               decision["voter_opinions"].push([i, 1])
         }
         for( i = 0; i < writeInRepeater.count; i++ ) {
            if( writeInRepeater.itemAt(i).checked ) {
               decision["voter_opinions"].push([contestantList.count + writeInCount, 1])
               decision["write_in_names"].push(writeInRepeater.itemAt(i).contestantName)
               writeInCount++
            }
         }

         if( decision["voter_opinions"].length === 0 )
            decision = {}
      }

      return decision
   }
   function setDecision(decision) {
      if( "voter_opinions" in decision && decision["voter_opinions"].length >= 1 ) {
         for( var opinion in decision["voter_opinions"] ) {
            opinion = decision["voter_opinions"][opinion]
            if( opinion[0] < contestantList.count )
               contestantRepeater.itemAt(opinion[0]).checked = true
            else {
               var button = writeInRepeater.itemAt(opinion[0] - contestantList.count)
               button.contestantName = decision["write_in_names"][opinion[0] - contestantList.count]
               button.checked = true
            }
         }
      }
   }
   
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
   Component {
      id: writeInDialog
      Dialog {
         title: qsTr("Enter a Write-In Candidate")
         standardButtons: StandardButton.Ok | StandardButton.Cancel
         modality: Qt.WindowModal
         width: window.width / 2

         property var acceptedCallback
         onAccepted: acceptedCallback(writeInName.text)
         property var rejectedCallback
         onRejected: rejectedCallback()

         function setText(text) { writeInName.text = text }

         TextField {
            id: writeInName
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter
            placeholderText: "Enter Write-In Candidate Name Here"
         }
      }
   }

   Loader {
      id: contestantButtonsGroup
      sourceComponent: winnerLimit > 1?
                          voteManyGroup : voteOneGroup
   }
   Loader {
      id: writeInDialogLoader
      sourceComponent: writeInSlots === 0? undefined : writeInDialog
   }

   Repeater {
      id: contestantRepeater
      model: contestantList
      delegate: SimpleButton {
         Layout.fillWidth: true
         Layout.fillHeight: true
         Layout.minimumWidth: textWidth
         Layout.preferredWidth: parent.width / parent.columns
         exclusiveGroup: contestantButtonsGroup.item
         text: "<b>" + name + "</b><br/><br/><i>" + breakLines(description) + "</i>"
         height: fontSize * 4.5

         property int contestantIndex: index
      }
   }
   Repeater {
      id: writeInRepeater
      model: writeInSlots
      delegate: SimpleButton {
         property string contestantName: placeHolderText
         property string placeHolderText: "Click to write-in"

         Layout.fillWidth: true
         Layout.fillHeight: true
         Layout.minimumWidth: textWidth
         Layout.preferredWidth: parent.width / parent.columns
         exclusiveGroup: contestantButtonsGroup.item
         text: "<b>" + contestantName + "</b><br/><br/>Write-In Candidate"
         height: fontSize * 4.5
         onClicked: {
            if( checked ) {
               if( contestantName === placeHolderText )
                  writeInDialogLoader.item.setText("")
               else
                  writeInDialogLoader.item.setText(contestantName)

               writeInDialogLoader.item.acceptedCallback = function(name) {
                  if( name !== "" )
                     contestantName = name
                  else
                     writeInDialogLoader.item.rejectedCallback()
               }
               writeInDialogLoader.item.rejectedCallback = function() {
                  contestantName = placeHolderText
                  checked = false
               }
               writeInDialogLoader.item.open()
            }
         }

         property int writeInIndex: index
      }
   }
}
