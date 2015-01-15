import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1
import QtQuick.Window 2.2

import Material 0.1

import "utils.js" as Utils

Item {
   id: guiContainer

   property real minimumWidth: uiStack.minimumWidth
   property real minimumHeight: header.height + uiStack.minimumHeight

   StackView {
      id: uiStack
      anchors {
         top: header.bottom
         bottom: parent.bottom
      }
      width: parent.width

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
               uiStack.push(assetsUi)
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
               uiStack.pop()
            }
            onOpenHistory: uiStack.push({item: historyUi, properties: {accountName: account, assetSymbol: symbol}})
         }
      }
      Component {
         id: historyUi
         HistoryLayout {
         }
      }
   }
}
