import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import "utils.js" as Utils
import org.BitShares.Types 1.0

import Material 0.1

ApplicationWindow {
   id: window
   visible: true
   width: 540
   height: 960
   initialPage: mainPage

   readonly property int orientation: window.width < window.height? Qt.PortraitOrientation : Qt.LandscapeOrientation
   property bool needsOnboarding: !( wallet.account && wallet.account.isRegistered )

   function connectToServer() {
      if( !wallet.connected )
         wallet.connectToServer("localhost", 6691, "user", "pass")
   }
   function showError(error) {
      errorText.text = qsTr("Error: ") + error
      return d.currentError = d.errorSerial++
   }
   function clearError(errorId) {
      if( d.currentError === errorId )
         errorText.text = ""
   }

   Component.onCompleted: {
      if( wallet.walletExists )
         wallet.openWallet()
      if( needsOnboarding )
         onboardLoader.sourceComponent = onboardingUi

      connectToServer()
   }
   pageStack.onPopped: if( pageStack.count === 1 ) wallet.lockWallet()

   theme {
      primaryColor: "#2196F3"
      backgroundColor: "#BBDEFB"
      accentColor: "#80D8FF"
   }

   QtObject {
      id: d
      property int errorSerial: 0
      property int currentError: -1
   }
   QtObject {
      id: visuals

      property real margins: 20
   }
   Timer {
      id: refreshPoller
      running: wallet.unlocked
      triggeredOnStart: true
      interval: 10000
      repeat: true
      onTriggered: {
         if( !wallet.connected )
            connectToServer()
         else
            wallet.sync()
      }
   }
   LightWallet {
      id: wallet

      function runWhenConnected(fn) {
         Utils.connectOnce(wallet.onConnectedChanged, fn, function() { return connected })
      }

      onErrorConnecting: {
         var errorId = showError(error)

         runWhenConnected(function() {
            clearError(errorId)
         })
      }
   }

   WelcomeLayout {
      id: mainPage
      title: qsTr("Welcome to BitShares")
      onPasswordEntered: {
         if( wallet.unlocked )
            return proceedIfUnlocked()

         Utils.connectOnce(wallet.onUnlockedChanged, proceedIfUnlocked)
         wallet.unlockWallet(password)
      }

      function proceedIfUnlocked() {
         if( !wallet.unlocked )
            return

         console.log("Finished welcome screen.")
         clearPassword()
         window.pageStack.push(assetsUi)
      }
   }
   Component {
      id: onboardingUi

      OnboardingLayout {
         onFinished: {
            pageStack.push(assetsUi)
            onboardLoader.sourceComponent = null
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
         onOpenHistory: window.pageStack.push(historyUi, {"accountName": account, "assetSymbol": symbol})
         onOpenTransfer: window.pageStack.push(transferUi)
      }
   }
   Component {
      id: historyUi

      HistoryLayout {
      }
   }
   Component {
      id: transferUi

      TransferLayout {
      }
   }
   Loader {
      id: onboardLoader
      anchors.fill: parent
      z: 20
   }
}
