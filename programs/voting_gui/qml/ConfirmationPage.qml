import QtQuick 2.3
import QtQuick.Controls 1.2

TaskPage {
   onBackClicked: window.previousPage()
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
   SimpleButton {
      id: ballotBoxButton
      width: textWidth + 30
      anchors.top: spinner.bottom
      anchors.topMargin: 30
      anchors.horizontalCenter: parent.horizontalCenter
      height: spinner.height / 2
      text: "Open Ballot Box"
      highlighted: true
      pulsing: true
      color: "#8800ff00"
      onClicked: Qt.openUrlExternally("http://review.iddnet.com:4006/ballot-box?voter-id="+bitshares.voterAddress)
   }

   states: [
      State {
         name: "SUBMITTING"
         PropertyChanges {
            target: statusText
            text: qsTr("Your ballot is now being cast. Please wait...")
         }
         PropertyChanges {
            target: spinner
            running: true
         }
         PropertyChanges {
            target: ballotBoxButton
            visible: false
         }
      },
      State {
         name: "FINISHED"
         PropertyChanges {
            target: statusText
            text: qsTr("Congratulations! Your ballot has been cast successfully. Thank you for using the Follow My " +
                       "Vote voting booth. You should be able to see your votes counted in the Ballot Box within a " +
                       "few seconds!")
         }
         PropertyChanges {
            target: spinner
            running: false
         }
         PropertyChanges {
            target: ballotBoxButton
            visible: true
         }
      }
   ]
}
