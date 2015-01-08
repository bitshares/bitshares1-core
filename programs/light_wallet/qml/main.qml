import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import "utils.js" as Utils
import org.BitShares.Types 1.0

ApplicationWindow {
   id: window
   visible: true
   width: 540
   height: 960
   color: visuals.backgroundColor
   minimumWidth: guiLoader.item.minimumWidth
   minimumHeight: guiLoader.item.minimumHeight

   readonly property int orientation: window.width < window.height? Qt.PortraitOrientation : Qt.LandscapeOrientation

   function connectToServer() {
      if( !wallet.connected )
         wallet.connectToServer("localhost", 6691)
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

      if( wallet.account && wallet.account.isRegistered ) {
         guiLoader.sourceComponent = normalUi
      } else {
         guiLoader.sourceComponent = onboardingUi
      }

      connectToServer()
   }

   QtObject {
      id: d
      property int errorSerial: 0
      property int currentError: -1
   }
   QtObject {
      id: visuals

      property color backgroundColor: "white"
      property color textColor: "#535353"
      property color lightTextColor: "#757575"
      property color buttonColor: "#28A9F6"
      property color buttonHoverColor: "#2BB4FF"
      property color buttonPressedColor: "#264D87"
      property color buttonTextColor: "white"
      property color errorGlowColor: "red"

      property real spacing: 40
      property real margins: 20

      property real textBaseSize: window.orientation === Qt.PortraitOrientation?
                                     window.height * .02 : window.width * .03
   }
   Timer {
      id: reconnectPoller
      running: !wallet.connected
      interval: 5000
      repeat: true
      onTriggered: connectToServer()
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

   Component {
      id: normalUi

      WalletGui {
         id: walletGui
      }
   }
   Component {
      id: onboardingUi

      OnboardingLayout {
         id: onboardingGui

         onFinished: guiLoader.sourceComponent = normalUi
      }
   }

   Loader {
      id: guiLoader
      anchors.fill: parent
   }

   statusBar: StatusBar {
      RowLayout {
         width: parent.width

         Label {
            id: statusText
            text: wallet.connected? qsTr("Connected") : qsTr("Disconnected")
         }
         Item { Layout.fillWidth: true }
         Label {
            id: errorText
         }
      }
   }
}
