import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

Item {
   id: header
   height: headerLayout.y + headerLayout.height + 10

   property real fontSize: 12
   property alias leftText: leftLabel.text
   property alias rightText: rightLabel.text

   signal clicked

   RowLayout {
      id: headerLayout
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.top: parent.top
      anchors.topMargin: 10

      Text {
         id: leftLabel
         Layout.preferredWidth: parent.width * 2/3
         color: "white"
         font.pointSize: fontSize
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
      Text {
         id: rightLabel
         Layout.preferredWidth: parent.width / 3
         color: "white"
         font.pointSize: fontSize
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }
   }

   MouseArea {
      anchors.fill: parent
      hoverEnabled: false
      onClicked: header.clicked()
   }
}
