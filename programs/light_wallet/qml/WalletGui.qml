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
      needsRegistration: !(wallet.account && wallet.account.isRegistered)

      function proceed() {
         if( Stack.status !== Stack.Active )
            return;

         console.log("Finished welcome screen.")
         clearPassword()
         push(assetsUi)
      }
      function registerAccount() {
         welcomeBox.state = "REGISTERING"
         Utils.connectOnce(wallet.account.isRegisteredChanged, proceed,
                           function() { return wallet.account.isRegistered })
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
      }
      
      onPasswordEntered: {
         if( wallet.unlocked && wallet.account && wallet.account.isRegistered )
            return proceed()

         // First time through; there's no wallet, no account, not registered. First, create the wallet.
         if( !wallet.walletExists ) {
            Utils.connectOnce(wallet.walletExistsChanged, function(walletWasCreated) {
               if( walletWasCreated ) {
                  registerAccount()
               } else {
                  //TODO: failed to create wallet. What now?
                  window.showError("Unable to create wallet. Cannot continue.")
                  welcomeBox.state = ""
               }
            })
            wallet.createWallet(username, password)
         } else if ( !wallet.account.isRegistered ) {
            //Wallet is created, so the account exists, but it's not registered yet. Take care of that now.
            registerAccount()
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
