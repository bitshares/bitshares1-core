import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import "utils.js" as Utils

StackView {
   id: walletGui

   property real minimumWidth: (!!currentItem)? currentItem.minimumWidth : 1
   property real minimumHeight: (!!currentItem)? currentItem.minimumHeight : 1

   initialItem: wallet.unlocked? [welcomeUi, assetsUi] : welcomeUi

   Component {
      id: welcomeUi
      WelcomeLayout {
         id: welcomeBox

         function proceed() {
            if( Stack.status !== Stack.Active )
               return;

            console.log("Finished welcome screen.")
            clearPassword()
            push(assetsUi)
         }

         onPasswordEntered: {
            if( wallet.unlocked )
               return proceed()

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
