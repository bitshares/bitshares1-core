import QtQuick 2.3

import Material 0.1

Dialog {
   id: backupDialog
   title: qsTr("Keep it secret. Keep it safe.")
   positiveButtonText: qsTr("Next")

   onShowingChanged: if( showing ) backupLayout.state = ""
   onRejected: close()
   onAccepted: {
      if( backupLayout.state === "verifying" ) {
         if( !wallet.verifyBrainKey(brainKeyField.text) ) {
            brainShaker.shake()
         } else {
            wallet.clearBrainKey()
            backupLayout.state = "open"
            close()
         }
      } else {
         backupLayout.state = "verifying"
      }
   }

   Column {
      id: backupLayout
      width: parent.width

      Label {
         id: detailLabel
         anchors {
            left: parent.left
            right: parent.right
            margins: visuals.margins
         }
         style: "body2"
         text: qsTr("A password has been generated to protect your funds. This password should be written down and " +
                    "stored somewhere safe. It can be used to recover your account if you forget your " +
                    "login password or lose this %1. Anyone with this password can spend your money and use your " +
                    "BitShares identity, so it should be kept in a safe place.").arg(deviceType())
         wrapMode: Text.WrapAtWordBoundaryOrAnywhere
         horizontalAlignment: Text.AlignJustify
      }
      TextField {
         id: brainKeyField
         anchors {
            left: parent.left
            right: parent.right
            margins: visuals.margins
         }
         width: parent.width - visuals.margins*2
         input {
            font.pixelSize: units.dp(12)
            readOnly: true
            text: wallet.brainKey.toUpperCase()
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhUppercaseOnly | Qt.ImhLatinOnly
         }
         transform: ShakeAnimation { id: brainShaker }
         helperText: qsTr("Write down this password, then click the Next button below")

         Component.onCompleted: forceActiveFocus()
      }
      states: [
         State {
            name: "verifying"
            extend: "open"
            PropertyChanges {
               target: backupDialog
               title: qsTr("Is it secret? Is it safe?")
               onRejected: backupLayout.state = "open"
               positiveButtonText: qsTr("Verify")
               negativeButtonText: qsTr("Back")
            }
            PropertyChanges {
               target: detailLabel
               text: qsTr("To make sure you've copied your password down correctly, please re-enter it below. " +
                          "After you have verified the password, it will be removed from your %1 and it cannot be " +
                          "recovered, so be careful not to lose it.").arg(deviceType())
            }
            PropertyChanges {
               target: brainKeyField
               text: ""
               input.readOnly: false
               helperText: qsTr("Enter your password now. Spaces and case do not matter")
               onAccepted: backupLayout.accepted()
            }
         }
      ]
      transitions: [
         Transition {
            SequentialAnimation {
               PropertyAnimation {
                  targets: [detailLabel,brainKeyField]
                  property: "opacity"
                  from: "1"; to: "0"
                  duration: 200
               }
               PropertyAction { target: detailLabel; property: "text" }
               PropertyAction { target: brainKeyField; properties: "helperText,text" }
               PropertyAnimation {
                  targets: [detailLabel,brainKeyField]
                  property: "opacity"
                  from: "0"; to: "1"
                  duration: 200
               }

            }
         }
      ]
   }
}
