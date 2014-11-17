import QtQuick 2.3
import QtQuick.Layouts 1.1

Rectangle {
   id: container
   color: "#d9d9d9"

   property variant verifiers: ["verifier"]
   property string account_name

   function processResponse(response) {
      if( response.response === null )
         //Request not yet processed. Keep waiting.
         return;

      response = response.response

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
         responsePollster.start()
      })
   }

   Timer {
      id: responsePollster
      interval: 1000
      repeat: true
      triggeredOnStart: true
      onTriggered: {
         console.log("Polling verification status....")
         bitshares.get_verification_request_status(account_name, verifiers, function(response) {
            processResponse(JSON.parse(response))
         })
      }
   }

   Rectangle {
       id: rotatingBox
       width: 50
       height: 50
       color: "lightsteelblue"
       anchors.centerIn: parent
       RotationAnimation {
           target: rotatingBox;
           from: 0;
           to: 360;
           duration: 500
           running: true
           loops: Animation.Infinite
       }
   }
}
