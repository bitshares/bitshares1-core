import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import "utils.js" as Utils

StackView {
   id: walletGui

   property real minimumWidth: welcomeBox.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: welcomeBox.Layout.minimumHeight + visuals.margins * 2

   initialItem: WelcomeLayout {
      id: welcomeBox
      anchors.fill: parent
      anchors.margins: visuals.margins
      firstTime: !wallet.walletExists

      function proceed() {
         console.log("Finished welcome screen.")
      }
      
      onPasswordEntered: {
         if( wallet.unlocked )
            return

         if( firstTime ) {
            Utils.connectOnce(wallet.walletExistsChanged, function(walletWasCreated) {
               if( walletWasCreated ) {
                  console.log("Wallet created!")
                  Utils.connectOnce(wallet.onAccountRegistered, proceed)

                  if( wallet.connected ) {
                     wallet.registerAccount()
                  } else {
                     // Not connected. Schedule for when we reconnect.
                     wallet.runWhenConnected(function() {
                        wallet.registerAccount()
                     })
                  }
               } else {
                  //TODO: failed to create wallet. What now?
                  window.showError("Unable to create wallet. Cannot continue.")
                  welcomeBox.state = ""
               }
            })
            welcomeBox.state = "REGISTERING"
            wallet.createWallet(username, password)
         } else {
            console.log("Unlocking wallet...")
            wallet.unlockWallet(password)
         }
      }

      Component.onCompleted: Utils.connectOnce(wallet.onUnlockedChanged, proceed, function() { return wallet.unlocked })
   }
}
