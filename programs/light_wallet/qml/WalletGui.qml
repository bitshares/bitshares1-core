import QtQuick 2.4
import QtQuick.Controls 1.3
import QtQuick.Layouts 1.1

StackView {
   id: walletGui

   property real minimumWidth: welcomeBox.Layout.minimumWidth + visuals.margins * 2
   property real minimumHeight: welcomeBox.Layout.minimumHeight + visuals.margins * 2

   initialItem: WelcomeLayout {
      id: welcomeBox
      anchors.fill: parent
      anchors.margins: visuals.margins
      firstTime: !wallet.walletExists
      
      onPasswordEntered: {
         if( firstTime ) {
            var onWalletCreated = function(walletWasCreated) {
               wallet.walletExistsChanged.disconnect(onWalletCreated)
               
               if( walletWasCreated ) {
                  console.log("Wallet created!")
               } else {
                  //TODO: Well, what now? Creating the wallet failed...
               }
            }
            
            wallet.walletExistsChanged.connect(onWalletCreated)
            wallet.createWallet(password)
         }
      }
   }
}
