import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import "utils.js" as Utils

StackView {
   id: walletGui

   property real minimumWidth: (!!currentItem)? currentItem.minimumWidth : 1
   property real minimumHeight: (!!currentItem)? currentItem.minimumHeight : 1

   initialItem: WelcomeLayout {
      id: welcomeBox
      firstTime: !wallet.walletExists

      function proceed() {
         console.log("Finished welcome screen.")
         clearPassword()
         push(assetsUi)
      }
      
      onPasswordEntered: {
         if( wallet.unlocked )
            return proceed()

         if( firstTime ) {
            Utils.connectOnce(wallet.walletExistsChanged, function(walletWasCreated) {
               if( walletWasCreated ) {
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
            wallet.unlockWallet(password)
         }
      }

      Stack.onStatusChanged: {
         if( Stack.status === Stack.Active )
            Utils.connectOnce(wallet.onUnlockedChanged, proceed, function() { return wallet.unlocked })
      }
   }

   Component {
      id: assetsUi
      AssetsLayout {
         onLockRequested: {
            wallet.lockWallet()
            walletGui.pop()
         }
      }
   }
}
