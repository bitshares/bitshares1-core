import QtQuick 2.3
import QtQuick.Controls 1.2

TaskPage {
   id: container
   onBackClicked: window.previousPage()
   onNextClicked: {
      var decision_list = Object.keys(decisions).map(function(key){return decisions[key]})
      console.log(JSON.stringify(decision_list))
      bitshares.submit_decisions(account_name, JSON.stringify(decision_list))
   }

   property var contests: []
   property var ballot

   Component.onCompleted: {
      bitshares.load_election()
      var json_contests = bitshares.get_voter_contests(account_name)
      for( var index in json_contests ) {
         contests.push(json_contests[index])
      }
      ballot = bitshares.get_voter_ballot(account_name)
      //Refresh the contest list and scroll back to the top
      contestList.reloadContests()
      contestList.contentY = 0
      contestList.returnToBounds()
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
      flickableItem.interactive: true

      ListView {
         id: contestList
         spacing: 5

         Component {
            id: contestDelegate
            Contest {
               property bool initialized: false

               width: parent.width - 20
               expanded: ListView.isCurrentItem
               fontSize: Math.max(1, container.height * .02)
               onDecisionChanged: {
                  //Immediately upon creation, the decision is changed to an empty object. Ignore this.
                  if( initialized ) {
                     if( "voter_opinions" in decision ) {
                        var stored_decision = decision
                        stored_decision["contest_id"] = id
                        stored_decision["ballot_id"] = ballot.id
                        decisions[id] = stored_decision
                     } else
                        delete decisions[id]
                  }
                  console.log(JSON.stringify(decisions))
               }
               Component.onCompleted: {
                  setDecision(decisions[id])
                  initialized = true
               }
            }
         }

         model: ListModel {dynamicRoles: true}
         delegate: contestDelegate

         function reloadContests() {
            model.clear()
            for( var contest in contests ) {
               //Convert tags in contest to a dictionary instead of a list of lists
               var tags = {}
               for( var tag in contests[contest]["tags"] )
                  tags[contests[contest]["tags"][tag][0]] = contests[contest]["tags"][tag][1]
               contests[contest]["tags"] = tags

               console.log(JSON.stringify(contests[contest]))

               model.append(contests[contest])
            }
         }
      }
   }
}
