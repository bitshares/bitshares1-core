import QtQuick 2.4
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.3

import "utils.js" as Utils
import org.BitShares.Types 1.0

import Material 0.1

ApplicationWindow {
   id: window
   visible: true
   width: 540
   height: 960
   initialPage: mainPage
   pageStack.visible: !onboardLoader.sourceComponent
   toolBar.visible: !onboardLoader.sourceComponent

   readonly property int orientation: window.width < window.height? Qt.PortraitOrientation : Qt.LandscapeOrientation
   property bool needsOnboarding: !( wallet.account && wallet.account.isRegistered )

   function connectToServer() {
      if( !wallet.connected )
         wallet.connectToServer("localhost", 6691, "user", "pass")
   }
   function showError(error, buttonName, buttonCallback) {
      snack.text = error
      if( buttonName && buttonCallback ) {
         snack.buttonText = buttonName
         snack.onClick.connect(buttonCallback)
         snack.onDissapear.connect(function() {
            snack.onClick.disconnect(buttonCallback)
         })
      } else
         snack.buttonText = ""
      snack.open()
      return d.currentError = d.errorSerial++
   }
   function clearError(errorId) {
      if( d.currentError === errorId )
         errorText.text = ""
   }
   function deviceType() {
      switch(Device.type) {
      case Device.phone:
      case Device.phablet:
         return "phone"
      case Device.tablet:
         return "tablet"
      case Device.desktop:
      default:
         return "computer"
      }
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

      property real margins: units.dp(16)
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
      onAccountChanged: if( account ) account.error.connect(showError)
      onNotification: showError(message)
   }

   View {
      id: criticalNotificationArea
      backgroundColor: "#F44336"
      height: units.dp(100)
      width: parent.width
      enabled: false
      z: -1

      //Show iff brain key is set, the main page is not active or transitioning out, and we're not already in an onboarding UI
      property bool active: wallet.brainKey.length > 0 &&
                            [Stack.Deactivating, Stack.Active].indexOf(mainPage.Stack.status) < 0 &&
                            !onboardLoader.sourceComponent

      Label {
         anchors.verticalCenter: parent.verticalCenter
         anchors.left:  parent.left
         anchors.right: criticalNotificationButton.left
         anchors.margins: visuals.margins
         text: qsTr("Your wallet has not been backed up. You should back it up as soon as possible.")
         color: "white"
         style: "subheading"
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      Button {
         id: criticalNotificationButton
         anchors.right: parent.right
         anchors.rightMargin: visuals.margins
         anchors.verticalCenter: parent.verticalCenter
         text: qsTr("Back Up Wallet")
         textColor: "white"

         onClicked: onboardLoader.sourceComponent = backupUi
      }

      states: [
         State {
            name: "active"
            when: criticalNotificationArea.active
            PropertyChanges {
               target: criticalNotificationArea
               enabled: true
            }
            PropertyChanges {
               target: pageStack
               anchors.topMargin: criticalNotificationArea.height
            }
         }
      ]
      transitions: [
         Transition {
            from: ""
            to: "active"
            reversible: true
            PropertyAnimation { target: pageStack; property: "anchors.topMargin"; easing.type: Easing.InOutQuad }
         }
      ]
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

      Stack.onStatusChanged: if( Stack.status === Stack.Active ) wallet.lockWallet()
   }
   Component {
      id: onboardingUi

      OnboardingLayout {
         onFinished: {
            pageStack.push(assetsUi)
            onboardLoader.sourceComponent = undefined
         }
      }
   }
   Component {
      id: backupUi

      BackupLayout {
         onFinished: onboardLoader.sourceComponent = undefined
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
         onOpenTransfer: {
            if( wallet.account.availableAssets.length )
               window.pageStack.push(transferUi)
            else
               showError(qsTr("You don't have any assets, so you cannot make a transfer."), qsTr("Refresh Balances"),
                         wallet.syncAllBalances)
         }
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
         onTransferComplete: window.pageStack.pop()
      }
   }
   Loader {
      id: onboardLoader
      anchors.fill: parent
   }
   Snackbar {
      id: snack
      duration: 5000
      enabled: opened
      onClick: opened = false
      z: 21
   }
}
