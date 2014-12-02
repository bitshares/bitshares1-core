import QtQuick 2.3
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2

TaskPage {
   id: container
   state: "SUBMITTING"

   property var verifiers: ["verifier"]
   property var registrars: ["registrar"]

   onBackClicked: window.previousPage()
   onNextClicked: {
      if( mayProceed() ) {
         window.nextPage()
      } else {
         cannotProceedAlert()
      }
   }
   nextButtonHighlighted: mayProceed()

   function mayProceed() {
      if( container.state === "SUBMITTING" || container.state === "POLLING" || container.state === "REJECTED" )
         return false
      return true
   }
   function cannotProceedAlert() {
      console.log("error")
      errorAnimation.restart()
   }
   function processResponse(response) {
      // @disable-check M126
      if( response.response == null ) {
         //Request not yet processed. Keep waiting.
         if( container.state !== "POLLING" ) {
            container.state = "POLLING"
         }
         return;
      }

      response = response.response
      responsePollster.stop()

      if( !response.accepted ) {
         console.log("Request rejected: " + JSON.stringify(response))
         container.state = "REJECTED"
      } else {
         bitshares.process_accepted_identity(account_name, JSON.stringify(response.verified_identity),
                                             function(acceptance_count)
                                             {
                                                if( acceptance_count > (verifiers.length / 2) ) {
                                                   container.state = "ACCEPTED"
                                                   bitshares.begin_registration(account_name, registrars)
                                                }
                                             })
      }
   }
   function startVerification() {
      if( !bitshares.initialized ) {
         console.log("Cannot verify account; backend is not yet initialized.")
         return;
      }

      if( account_name == "" ) {
         console.log("Verification begins")
         account_name = bitshares.create_voter_account()
      } else {
         console.log("Resubmitting verification request.")
      }

      bitshares.begin_verification(window, account_name, verifiers, function(response) {
         console.log("Verification submitted: " + response)
         container.state = "POLLING"
         processResponse(JSON.parse(response))
      })
   }

   Stack.onStatusChanged: {
      if( Stack.status === Stack.Active ) {
         if( container.state === "REJECTED" ) {
            container.state = "SUBMITTING"
         }
         startVerification()
      }
   }

   Connections {
      target: bitshares
      onRegistered: {
         container.state = "REGISTERED"
      }
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

      SequentialAnimation {
         id: errorAnimation
         PropertyAnimation { target: statusText; property: "color"; from: "white"; to: "red"; duration: 200 }
         PropertyAnimation { target: statusText; property: "color"; from: "red"; to: "white"; duration: 200 }
      }
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
                       "Please wait while your identity is verified and your ballot is prepared.")
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
            running: true
         }
         PropertyChanges {
            target: statusText
            text: qsTr("Your identity has been verified successfully. An anonymous voting identity is now being " +
                       "registered, which will allow you to cast your votes anonymously. You may proceed to the next " +
                       "step while waiting for this registration.")
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
      },
      State {
         name: "REGISTERED"
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
            text: qsTr("Your identity has been accepted and anonymously registered to vote. Please proceed to the " +
                       "next step.")
         }
      }
   ]
}
