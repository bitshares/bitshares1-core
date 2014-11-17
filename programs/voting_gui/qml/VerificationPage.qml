import QtQuick 2.3
import QtQuick.Layouts 1.1

TaskPage {
   id: container

   property variant verifiers: ["verifier"]
   property string account_name

   function processResponse(response) {
      // @disable-check M126
      if( response.response == null ) {
         //Request not yet processed. Keep waiting.
         return;
      }

      response = response.response
      responsePollster.stop()

      if( !response.accepted ) {
         console.log("Request rejected: " + JSON.stringify(response))
      } else {
         console.log("Request accepted: " + JSON.stringify(response))
      }
   }

   Component.onCompleted: {
      if( !bitshares.initialized ) {
         console.log("Cannot verify account; backend is not yet initialized.")
         return;
      }

      console.log("Verification begins")
      account_name = bitshares.create_voter_account()
      bitshares.begin_verification(window, account_name, verifiers, function(response) {
         console.log("Verification submitted: " + response)
         processResponse(JSON.parse(response))
      })
   }

   Timer {
      id: responsePollster
      interval: 10000
      repeat: true
      running: true
      onTriggered: {
         console.log("Polling verification status....")
         bitshares.get_verification_request_status(account_name, verifiers, function(response) {
            processResponse(JSON.parse(response))
         })
      }
   }
   Text {
      id: statusText
      anchors.centerIn: parent
      text: qsTr("Your information is being securely tranmitted to official election verification officials. Please" +
                 " wait...");
      color: "white"
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      width: parent.width / 2
      font.pointSize: Math.max(1, parent.height / 50)
   }
   Spinner {
      anchors.top: statusText.bottom
      anchors.horizontalCenter: parent.horizontalCenter
      height: parent.height / 10
   }
}
