import QtQuick 2.3
import QtQuick.Controls 1.2

TaskPage {
   id: container
   onBackClicked: window.previousPage()
   onNextClicked: {
      window.nextPage()
      return
   }

   property var contests: []
   property var ballot

   Component.onCompleted: {
      bitshares.load_election()
      var json_contests = bitshares.get_voter_contests()
      for( var index in json_contests ) {
         contests.push(json_contests[index])
      }
      ballot = bitshares.get_voter_ballot()
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
                     if( "voter_opinions" in decision && decision.voter_opinions.length > 0 ) {
                        var stored_decision = decision
                        stored_decision["contest_id"] = id
                        stored_decision["ballot_id"] = ballot.id
                        decisions[id] = stored_decision
                        // @disable-check M126 -- We want the type coercion on this comparison.
                        if( !("vote many limit" in tags) || decision.voter_opinions.length == tags["vote many limit"] ) {
                           expanded = false
                           var nextItem = contestList.itemAt(x+width/2, y+height+collapsedHeight/2)
                           if( nextItem ) {
                              nextItem.expanded = true
                           } else {
                              var reposition = function() {
                                 contestList.positionViewAtEnd()
                                 collapseComplete.disconnect(reposition)
                              }
                              collapseComplete.connect(reposition)
                           }
                        }
                     } else
                        delete decisions[id]
                  }
               }
               onExpansionComplete: contestList.positionViewAtIndex(index, ListView.Contain)
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

               model.append(contests[contest])
            }
         }
      }
   }
}
