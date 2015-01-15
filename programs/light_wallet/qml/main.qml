import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import "utils.js" as Utils
import org.BitShares.Types 1.0

import Material 0.1
import Material.Transitions 0.1 as T

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
   pageStack.onPagePopped: if( pageStack.count === 1 ) wallet.lockWallet()

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

      property color textColor: "#535353"
      property color lightTextColor: "#757575"
      property color buttonColor: "#28A9F6"
      property color buttonHoverColor: "#2BB4FF"
      property color buttonPressedColor: "#264D87"
      property color buttonTextColor: "white"

      property real spacing: 40
      property real margins: 20

      property real textBaseSize: window.orientation === Qt.PortraitOrientation?
                                     window.height * .02 : window.width * .03
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
      anchors.fill: parent
      //Make a null transition for initial page; there's no reason to animate the first page appearing.
      transition: T.PageTransition {function transitionTo(){}}
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
         anchors.fill: parent
         onLockRequested: {
            wallet.lockWallet()
            uiStack.pop()
         }
         onOpenHistory: window.pageStack.push(historyUi, {"accountName": account, "assetSymbol": symbol})
      }
   }
   Component {
      id: historyUi

      HistoryLayout {
      }
   }
   Loader {
      id: onboardLoader
      anchors.fill: parent
      z: 20
   }
}
