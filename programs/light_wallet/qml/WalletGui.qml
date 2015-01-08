import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

import "utils.js" as Utils

Item {
   id: guiContainer

   property real minimumWidth: uiStack.minimumWidth
   property real minimumHeight: assetsHeader.height + uiStack.minimumHeight

   Rectangle {
      id: assetsHeader
      width: parent.width
      height: assetHeaderRow.height + visuals.margins * 2
      color: "#22000000"

      RowLayout {
         id: assetHeaderRow
         width: parent.width - visuals.margins * 2
         y: visuals.margins
         anchors.horizontalCenter: parent.horizontalCenter

         Button {
            style: WalletButtonStyle{}
            text: qsTr("Lock")
            onClicked: {
               wallet.lockWallet()
               uiStack.pop(welcomeUi)
            }
         }
         Item { Layout.fillWidth: true }
         Label {
            color: visuals.textColor
            font.pixelSize: visuals.textBaseSize * 1.1
            text: wallet.account.name
         }
         Item { Layout.fillWidth: true }
      }
      Rectangle { height: 1; width: parent.width; color: visuals.lightTextColor; anchors.bottom: parent.bottom }
   }

   StackView {
      id: uiStack
      anchors {
         top: assetsHeader.bottom
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
         }
      }
   }
}
