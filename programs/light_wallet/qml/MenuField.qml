import QtQuick 2.3
import QtQuick.Window 2.2
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.3
import QtGraphicalEffects 1.0

import Qt.labs.settings 1.0

import "utils.js" as Utils
import org.BitShares.Types 1.0

import Material 0.1
import Material.ListItems 0.1

View {
   id: spinBox
   height: units.dp(32)
   width: spinBoxContents.implicitWidth
   backgroundColor: "white"
   elevation: 1
   
   property alias model: listView.model
   readonly property string selectedText: listView.currentItem.text
   property alias selectedIndex: listView.currentIndex
   property int maxVisibleItems: 4

   signal itemSelected(int index)
   
   Ink {
      anchors.fill: parent
      onClicked: {
         listView.positionViewAtIndex(listView.currentIndex, ListView.Center)
         var offset = listView.currentItem.itemLabel.mapToItem(menu, 0, 0)
         menu.open(label, -offset.x, -offset.y)
      }
   }
   RowLayout {
      id: spinBoxContents
      height: parent.height
      width: parent.width
      
      Item { Layout.minimumWidth: units.dp(16) }
      Label {
         id: label
         Layout.fillWidth: true
         anchors.verticalCenter: parent.verticalCenter
         text: listView.currentItem.text
         style: "subheading"
         elide: Text.ElideRight
      }
      Icon {
         id: dropDownIcon
         anchors.verticalCenter: parent.verticalCenter
         name: "navigation/arrow_drop_down"
         Layout.maximumWidth: units.dp(24)
         fillMode: Image.PreserveAspectFit
      }
   }
   
   Dropdown {
      id: menu
      anchor: Item.TopLeft
      width: Math.max(units.dp(56*2), Math.min(spinBox.width - 2 * x, listView.contentWidth))
      //If there are more than max items, show an extra half item so it's clear the user can scroll
      height: Math.min(maxVisibleItems*units.dp(48) + units.dp(24), listView.contentHeight)

      Scrollbar {
         flickableItem: listView
      }
      ListView {
         id: listView
         width: menu.width
         height: menu.height
         interactive: true
         model: ["Apples","Oranges","Bananas","Grapefruit","Kiwi","Lemon","Watermelon"]
         delegate: Standard {
            id: delegateItem
            text: modelData
            
            onTriggered: {
               spinBox.itemSelected(index)
               listView.currentIndex = index
               menu.close()
            }
         }
      }
   }
}
