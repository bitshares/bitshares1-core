import QtQuick 2.3
import QtQuick.Controls 1.2

TaskPage {
   backButtonVisible: false
   nextButtonVisible: false
   state: "SUBMITTING"

   function submissionComplete() {
      state = "FINISHED"
   }

   Stack.onStatusChanged: {
      if( Stack.status === Stack.Activating ) {
         var decisionList = Object.keys(decisions).map(function(key){return decisions[key]})
         bitshares.decisions_submitted.connect(submissionComplete)
         bitshares.submit_decisions(JSON.stringify(decisionList))
      }
   }

   Text {
      id: statusText
      anchors.centerIn: parent
      color: "white"
      width: parent.width * 2/3
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      horizontalAlignment: Text.AlignHCenter
      font.pointSize: Math.max(1, window.height * .03)
   }
   Spinner {
      id: spinner
      anchors.top: statusText.bottom
      anchors.topMargin: 30
      anchors.horizontalCenter: parent.horizontalCenter
      height: parent.height / 10
   }

   states: [
      State {
         name: "SUBMITTING"
         PropertyChanges {
            target: statusText
            text: qsTr("Your votes are now being cast. Please wait...")
         }
         PropertyChanges {
            target: spinner
            running: true
         }
      },
      State {
         name: "FINISHED"
         PropertyChanges {
            target: statusText
            text: qsTr("Congratulations! Your votes have been cast successfully. Thank you for using the Follow My " +
                       "Vote voting booth.")
         }
         PropertyChanges {
            target: spinner
            running: false
         }
      }
   ]
}
