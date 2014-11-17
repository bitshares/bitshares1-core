import QtQuick 2.3
import QtQuick.Layouts 1.1

Rectangle {
   id: container
   color: "#d9d9d9"

   Component.onCompleted: {
      var verifiers = ["verifier"]

      console.log("Verification begins")
      var account = bitshares.create_voter_account()
      bitshares.begin_verification(window, account, verifiers, function(response) {
         response = JSON.parse(response)
         console.log("Verification submitted: " + response)
      })
   }

   Connections {
      target: bitshares
      onError: console.log("Error from backend: " + errorString)
   }
}
