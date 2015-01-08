import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1
import QtQuick.Window 2.2

import "utils.js" as Utils

Item {
   id: guiContainer

   property real minimumWidth: Math.max(uiStack.minimumWidth, header.minimumWidth)
   property real minimumHeight: header.height + uiStack.minimumHeight

   Rectangle {
      id: header
      width: parent.width
      height: assetHeaderRow.height + visuals.margins * 2
      color: "#22000000"

      property real minimumWidth: drawerButton.width + accountNameLabel.width*2 + visuals.margins * 2

      Item {
         id: assetHeaderRow
         width: parent.width - visuals.margins * 2
         height: drawerButton.height
         y: visuals.margins
         anchors.horizontalCenter: parent.horizontalCenter

         Button {
            id: drawerButton
            anchors.left: parent.left
            style: WalletButtonStyle{}
            width: Screen.pixelDensity * 10
            height: Screen.pixelDensity * 8
            text: "···"
            visible: wallet.unlocked
            onClicked: {
               wallet.lockWallet()
               uiStack.pop(welcomeUi)
            }
         }
         Label {
            id: accountNameLabel
            anchors.centerIn: parent
            color: visuals.textColor
            font.pixelSize: visuals.textBaseSize * 1.1
            text: wallet.account.name
         }
      }
      Rectangle { height: 1; width: parent.width; color: visuals.lightTextColor; anchors.bottom: parent.bottom }
   }

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
         }
      }
   }
}
