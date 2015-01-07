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
                  Utils.connectOnce(wallet.onErrorRegistering, function(reason) {
                     //FIXME: Do something much, much better here...
                     console.log("Can't register: " + reason)
                  })

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
            Utils.connectOnce(wallet.onUnlockedChanged, proceed, function() { return wallet.unlocked })
            wallet.unlockWallet(password)
         }
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
