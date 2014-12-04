import QtQuick 2.3
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.2

Rectangle {
   id: card
   radius: width * 0.03504672897
   implicitWidth: height * 1.5857725083

   property url photoUrl
   property string votingAddress
   property string fullName
   property string streetAddress
   property string birthDate

   QtObject {
      id: d
      property real textSize: idVoterIdLabel.font.pixelSize * .75
      property real wideMargin: card.radius
      property real narrowMargin: card.radius / 2
   }
   
   Text {
      id: idFollowMyVoteLabel
      text: "Follow My Vote"
      anchors.top: parent.top
      anchors.bottom: idPhoto.top
      anchors.margins: d.narrowMargin
      anchors.horizontalCenter: parent.horizontalCenter
      font.pixelSize: height
   }
   Text {
      id: idVoterIdLabel
      text: "Voter Identification"
      anchors.top: idFollowMyVoteLabel.bottom
      anchors.topMargin: idFollowMyVoteLabel.height / 10
      anchors.horizontalCenter: idFollowMyVoteLabel.horizontalCenter
      font.pixelSize: idFollowMyVoteLabel.font.pixelSize / 2.5
   }
   Image {
      id: idPhoto
      fillMode: Image.PreserveAspectCrop
      anchors.bottom: parent.bottom
      anchors.left: parent.left
      anchors.margins: d.wideMargin
      height: parent.height * 3/4
      width: height * .6
      source: photoUrl
   }
   ColumnLayout {
      anchors {
         left: idPhoto.right
         right: parent.right
         top: idVoterIdLabel.bottom
         bottom: parent.bottom
         margins: d.wideMargin
      }

      Text {
         id: voterIdText
         text: "Voter Key:\n" + votingAddress
         font.pixelSize: d.textSize * .9
      }
      Text {
         id: voterName
         text: fullName
         font.pixelSize: d.textSize
      }
      Text {
         id: voterBirthDate
         text: birthDate
         font.pixelSize: d.textSize
      }
      Text {
         id: voterAddress
         text: streetAddress
         font.pixelSize: d.textSize
      }
   }
}
