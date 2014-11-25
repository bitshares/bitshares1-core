import QtQuick 2.3
import QtQuick.Controls 1.2

TaskPage {
   id: container
   onBackClicked: window.previousPage()

   property var contests: []
   property var ballot

   Component.onCompleted: {
      bitshares.load_election()
      var json_contests = bitshares.get_voter_contests("joe")
      for( var index in json_contests ) {
         contests.push(json_contests[index])
      }
      ballot = bitshares.get_voter_ballot("joe")
   }

   Text {
      id: titleText
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.margins: 40
      width: parent.width * .9
      font.pointSize: Math.max(1, parent.height * .03)
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      text: ballot.title + ": " + ballot.description
      color: "white"
   }

   ScrollView {
      anchors {
         top: titleText.bottom
         left: parent.left
         right: parent.right
         margins: 40
      }
      height: parent.height - y - buttonAreaHeight
   }
}
