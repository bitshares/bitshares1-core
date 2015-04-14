import QtQuick 2.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.3 as Controls3
import QtGraphicalEffects 1.0

import Qt.labs.settings 1.0

import "utils.js" as Utils
import org.BitShares.Types 1.0

import Material 0.1
import Material.ListItems 0.1
import Material.Extras 0.1 as Extras

ApplicationWindow {
   id: window
   visible: true
   width: units.dp(1200)
   height: units.dp(1000)
   Settings {
      id: persist

      property alias width: window.width
      property alias height: window.height
      property alias x: window.x
      property alias y: window.y
      property string guid: Extras.Utils.generateID()
   }

   property alias lockAction: __lockAction
   property alias payAction: __payAction

   Component.onCompleted: {
      onboardLoader.parent = Extras.Utils.findRootChild(onboardLoader, "dialogOverlayLayer")
      lockScreen.parent = Extras.Utils.findRootChild(lockScreen, "dialogOverlayLayer")

      if( wallet.walletExists )
         wallet.openWallet()
      if( !( wallet.accountNames.length && wallet.accounts[wallet.accountNames[0]].isRegistered ) )
         onboardLoader.sourceComponent = onboardingUi
      else
         pageStack.push({item: assetsUi, properties: {accountName: wallet.accountNames[0]}})

      window.connectToServer()
      checkManifest()
      lockScreen.focus()
   }

   function showMessage(error, buttonName, buttonCallback) {
      if( buttonName && buttonCallback ) {
         snack.buttonText = buttonName
         snack.buttonCallback = buttonCallback
      } else
         snack.buttonText = ""
      snack.open(error)
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
   function format(amount, symbol) {
      var l = Qt.locale()
      var rx = new RegExp("(\\" + l.decimalPoint + ")?0*$")
      return Number(amount).toLocaleString(l, 'f', Math.max(wallet.getDigitsOfPrecision(symbol), 1)).replace(rx, '')
   }
   function connectToServer() {
      if( !wallet.connected )
         wallet.connectToServer("nathanhourt.com", 5656, "BTS5LyQycuMEdo6Dxx1XqYp24KV3fVoFKuXJMXTj3x7xJsis3C3EZ")
   }
   function checkManifest() {
      if( AppName === "lw_xts" )
         return;
      var version = Qt.application.version
      var platform = Qt.platform.os
      if( PlatformName )
         platform = PlatformName
      var xhr = new XMLHttpRequest()
      var url = encodeURI(ManifestUrl + "?uuid="+persist.guid+"&version="+AppName+"/"+version+"&platform="+platform)
      xhr.open("GET", url, true)
      xhr.send()
   }
   function openTransferPage(args) {
      if( wallet.accounts[args.accountName].availableAssets.length )
         window.pageStack.push({item: transferUi, properties: args})
      else
         showMessage(qsTr("You don't have any assets, so you cannot make a transfer."), qsTr("Refresh Balances"),
                     wallet.syncAllBalances)
   }
   function openOrderForm(args) {
      if( wallet.accounts[args.accountName].availableAssets.length )
         window.pageStack.push({item: orderUi, properties: args})
      else
         showMessage(qsTr("You don't have any assets, so you cannot place a market order."), qsTr("Refresh Balances"),
                     wallet.syncAllBalances)
   }

   AppTheme {
      id: theme
      primaryColor: "#2196F3"
      backgroundColor: "#BBDEFB"
      accentColor: "#80D8FF"
   }
   Action {
      id: __lockAction
      name: qsTr("Lock Wallet")
      iconName: "action/lock"
      onTriggered: wallet.lockWallet()
   }
   Action {
      id: __payAction
      name: qsTr("Send Payment")
      iconName: "action/payment"
      onTriggered: openTransferPage({accountName: wallet.accountNames[0]})
   }
   Controls3.Action {
      id: __quitAction
      text: "Quit"
      shortcut: StandardKey.Quit
      onTriggered: Qt.quit()
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

      onConnectedChanged: {
         if( !connected ) {
            showMessage(qsTr("Connection to server lost. Retrying..."))
            window.connectToServer()
         }
      }
      onErrorConnecting: {
         showMessage(error)
      }
      onNotification: showMessage(message)
   }

   LockScreen {
      id: lockScreen
      width: window.width
      height: window.height
      backgroundColor: Theme.backgroundColor
      z: 1
      onPasswordEntered: {
         if( wallet.unlocked )
            return proceedIfUnlocked()

         wallet.unlockWallet(password)
      }

      states: [
         State {
            name: "locked"
            when: !wallet.unlocked
            PropertyChanges {
               target: lockScreen
               x: 0
            }
         },
         State {
            name: "unlocked"
            when: wallet.unlocked
            PropertyChanges {
               target: lockScreen
               x: window.width
            }
         }
      ]
      transitions: [
         Transition {
            from: "unlocked"
            to: "locked"
            SequentialAnimation {
               PropertyAnimation {
                  target: lockScreen
                  duration: 500
                  property: "x"
                  easing.type: Easing.OutBounce
               }
               ScriptAction { script: lockScreen.focus() }
            }
         },
         Transition {
            from: "locked"
            to: "unlocked"
            SequentialAnimation {
               PropertyAnimation {
                  target: lockScreen
                  duration: 500
                  property: "x"
                  easing.type: Easing.InQuad
               }
               ScriptAction { script: lockScreen.clearPassword() }
            }
         }
      ]
   }

   View {
      id: criticalNotificationArea
      y: toolbar.height
      backgroundColor: "#F44336"
      height: units.dp(100)
      width: parent.width
      enabled: false
      z: -5

      //Show iff brain key is set, the main page is not active or transitioning out, and we're not already in an onboarding UI
      property bool active: wallet.brainKey.length > 0

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

         onClicked: backupUi.show()
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

   Component {
      id: assetsUi

      AssetsLayout {
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
   Component {
      id: transferUi

      TransferLayout {
         accountName: wallet.accountNames[0]
         onTransferComplete: window.pageStack.pop()
      }
   }
   Component {
      id: orderUi

      OrderForm {
      }
   }
   Component {
      id: onboardingUi

      OnboardingLayout {
         onFinished: {
            pageStack.push({item: assetsUi, properties: {accountName: wallet.accountNames[0]}, immediate: true})
            onboardLoader.sourceComponent = undefined
         }
      }
   }
   BackupLayout {
      id: backupUi
      minimumWidth: window.width * 2/3
   }
   Loader {
      id: onboardLoader
      anchors.fill: parent
      z: 2
   }
   Snackbar {
      id: snack
      duration: 5000
      enabled: opened
      onClicked: opened = false
      z: 21
   }
}
