import QtQuick 2.3
import QtQuick.Controls 1.2

TaskPage {
   id: container
   backButtonVisible: false
   nextButtonHighlighted: true

   onNextClicked: window.nextPage()

   Text {
      anchors.centerIn: parent
      width: parent.width / 2
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      color: "white"
      font.pointSize: Math.max(1, parent.height / 40)
      text: qsTr("Welcome to the <i>Follow My Vote</i> Voting Booth.<br/><br/>This application will securely register your " +
                 "identity for anonymous voting, record and submit your votes, and provide you with an ID which " +
                 "may be used to verify that your vote was cast as intended and counted as cast.<br/><br/>You will need a " +
                 "Government-Issued ID card in order to proceed. This information is " +
                 "required to verify your identity and determine which ballot you should receive. This information " +
                 "cannot be reassociated with your votes.<br/><br/>Please click the <i>Next</i> button below when you are " +
                 "ready to proceed.")
   }
}
