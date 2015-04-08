import QtQuick 2.3
import QtQuick.Layouts 1.1

import Material 0.1

ColumnLayout {
   id: pullToRefresh
   y: Math.min(-height - view.contentY - spacing, spacing)
   anchors.horizontalCenter: parent.horizontalCenter
   spacing: units.dp(10)
   height: units.dp(100)

   property Flickable view
   property alias text: pullToRefreshLabel.text
   readonly property bool fullyPulled: pullToRefreshIndicator.value === 1

   signal triggered

   Connections {
      target: view
      onDragEnded: if( fullyPulled ) pullToRefresh.triggered()
   }

   Label {
      id: pullToRefreshLabel
      style: "headline"
      Layout.alignment: Qt.AlignHCenter
   }
   ProgressCircle {
      id: pullToRefreshIndicator
      Layout.alignment: Qt.AlignHCenter
      Layout.fillHeight: true
      width: height
      indeterminate: false
      value: Math.max(0, Math.min(1, (view.contentY + pullToRefreshLabel.height)
                                  / -pullToRefresh.height))
   }
}
