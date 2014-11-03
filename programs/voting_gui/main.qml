import QtQuick 2.3
import QtQuick.Controls 1.2

ApplicationWindow {
    visible: true
    width: 1280
    height: 1024
    title: qsTr("Voting Booth")

    Text {
        text: bitshares.info
        anchors.centerIn: parent
    }
}
