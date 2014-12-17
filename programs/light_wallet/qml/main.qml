import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Controls.Styles 1.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1

import org.BitShares.Types 1.0

ApplicationWindow {
   id: window
   visible: true
   width: 540
   height: 960
   color: visuals.backgroundColor

   readonly property int orientation: window.width < window.height? Qt.PortraitOrientation : Qt.LandscapeOrientation

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

   LightWallet {
      id: wallet
   }

   WelcomeLayout {
      id: welcomeBox
      anchors.fill: parent
      anchors.margins: visuals.margins
      firstTime: !wallet.walletExists
   }
}
