import QtQuick 2.3
import QtQuick.Layouts 1.1

TaskPage {
   id: container
   state: "SUBMITTING"

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
         container.state = "REJECTED"
      } else {
         console.log("Request accepted: " + JSON.stringify(response))
         container.state = "ACCEPTED"
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
         container.state = "POLLING"
         processResponse(JSON.parse(response))
      })
   }

   Timer {
      id: responsePollster
      interval: 10000
      repeat: true
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
      color: "white"
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      width: parent.width / 2
      font.pointSize: Math.max(1, parent.height / 50)
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
            target: responsePollster
            running: false
         }
         PropertyChanges {
            target: spinner
            running: true
         }
         PropertyChanges {
            target: statusText
            text: qsTr("Your information is being securely transmitted to election identity verification officials. " +
                       "Please wait...")
         }
      },
      State {
         name: "POLLING"
         PropertyChanges {
            target: responsePollster
            running: true
         }
         PropertyChanges {
            target: spinner
            running: true
         }
         PropertyChanges {
            target: statusText
            text: qsTr("The election identity verification officials are currently processing your information. " +
                       "When they finish, their response will be displayed here. You may proceed to the next step " +
                       "while waiting for their response.")
         }
      },
      State {
         name: "ACCEPTED"
         PropertyChanges {
            target: responsePollster
            running: false
         }
         PropertyChanges {
            target: spinner
            running: false
         }
         PropertyChanges {
            target: statusText
            text: qsTr("Your identity has been verified successfully. Please proceed to the next step.")
         }
         PropertyChanges {
            target: container
            nextButtonHighlighted: true
         }
      },
      State {
         name: "REJECTED"
         PropertyChanges {
            target: responsePollster
            running: false
         }
         PropertyChanges {
            target: spinner
            running: false
         }
         PropertyChanges {
            target: statusText
            text: qsTr("The election identity officials have rejected your identity. Please return to the previous " +
                       "step, correct any problems, and resubmit your request.")
         }
         PropertyChanges {
            target: container
            backButtonHighlighted: true
         }
      }

   ]
}
