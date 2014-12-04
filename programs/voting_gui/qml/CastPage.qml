import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

TaskPage {
   onBackClicked: window.previousPage()
   onNextClicked: window.nextPage()
   nextButtonHighlighted: true

   function describeDecision(contest, decision) {
      var tags = contest.tags
      var nominee = ""

      switch(tags["decision type"]) {
      case "vote one":
         if( decision.voter_opinions[0][0] < contest.contestants.length )
            return contest.contestants[decision.voter_opinions[0][0]].name
         else
            return decision.write_in_names[decision.voter_opinions[0][0] - contest.contestants.length]
      case "vote yes/no":
         switch(tags["contest type"]) {
         case "Judicial":
            nominee = contest.contestants[0].name
            return (decision.voter_opinions[0][1] === 1? "Approve " : "Disapprove ") + nominee
         case "Measures Submitted To The Voters":
            if( decision.voter_opinions[0][1] === 1 )
               return "Yes"
            return "No"
         }
         break
      case "vote many":
         for( var opinion in decision.voter_opinions ) {
            if( decision.voter_opinions[opinion][0] < contest.contestants.length )
               nominee += ", " + contest.contestants[decision.voter_opinions[opinion][0]].name
            else
               nominee += ", " + decision.write_in_names[decision.voter_opinions[opinion][0] - contest.contestants.length]
         }
         if( nominee.length > 2 )
            nominee = nominee.substr(2)
         return nominee
      }

      return "Unknown contest type."
   }
   function constructDecisionsModel() {
      return Object.keys(decisions).map(function(decision_id) {
         var decision = decisions[decision_id]
         var contest = bitshares.get_contest_by_id(decision.contest_id)
         var tags = {}
         for( var tag in contest.tags )
            tags[contest.tags[tag][0]] = contest.tags[tag][1]
         contest.tags = tags
         return {"contest": contest, "decision": decision}
      })
   }

   Stack.onStatusChanged: {
      if( Stack.status === Stack.Activating )
         //Force reloading this model on every activation
         decisionList.model = constructDecisionsModel()
   }

   ColumnLayout {
      anchors {
         top: parent.top
         left: parent.left
         right: parent.right
      }
      height: parent.height - y - buttonAreaHeight
      spacing: 20

      Item { Layout.preferredHeight: 20 }
      Text {
         id: instructionText
         Layout.preferredWidth: parent.width - 40
         Layout.alignment: Qt.AlignCenter
         color: "white"
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
         font.pointSize: window.height * .02
         text: qsTr("Your votes have not yet been cast. Please review them below and go back to make any " +
                    "necessary revisions, then click the Next button below to cast your votes.")
      }
      ScrollView {
         Layout.fillHeight: true
         Layout.preferredWidth: parent.width * 2/3
         Layout.alignment: Qt.AlignCenter
         anchors.horizontalCenter: parent.horizontalCenter
         flickableItem.interactive: true
         y: 40

         ListView {
            id: decisionList
            spacing: 10
            delegate: ContestHeader {
               width: parent.width
               leftText: model.modelData.contest.tags.name
               rightText: describeDecision(model.modelData.contest, model.modelData.decision)
               fontSize: window.height * .02
            }
         }
      }
      Item { Layout.fillHeight: true }
   }
}
