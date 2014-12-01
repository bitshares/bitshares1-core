import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Item {
   id: content

   function breakLines(text) {
      return text.replace("\n", "<br/>")
   }
   function dropLineBreaks(text) {
      return text.replace("\n", " ")
   }

   Text {
      id: instructionText
      anchors.top: parent.top
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.leftMargin: contestContainer.radius / 2
      anchors.rightMargin: anchors.leftMargin
      color: "white"
      font.pointSize: fontSize * .75
      wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      text: {
         switch( tags["decision type"] ) {
         case "vote one":
            return "Choose a candidate below."
         case "vote many":
            return "Choose at most " + tags["vote many limit"] + " candidates below."
         case "vote yes/no":
            switch( tags["contest type"] ) {
            case "Judicial":
               return "Shall " + contestants.get(0).description + " "
                     + contestants.get(0).name + " be elected to the office for the term provided by law?"
            case "Measures Submitted To The Voters":
               return description + "<br/>" + dropLineBreaks(contestants.get(0).description)
                     + "<br/><br/>Do you wish to approve this measure?"
            }
         }
      }
   }
   GridLayout {
      anchors.top: instructionText.bottom
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.margins: contestContainer.radius / 2
      columns: 4
      
      ExclusiveGroup { id: contestantButtonsGroup }
      Repeater {
         model: contestants
         delegate: Button {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: implicitWidth
            exclusiveGroup: contestantButtonsGroup
            checkable: true
            checked: false
            text: "<b>" + name + "</b><br/><br/>" + breakLines(description)
         }
      }
   }
}
