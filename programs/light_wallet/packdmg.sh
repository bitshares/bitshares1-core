#!/bin/bash

title="BitShares_Light_Wallet"
applicationName="BitShares Light Wallet"
source="dmg_pack_dir"
macdeployqt="/usr/local/opt/qt5/bin/macdeployqt"
iconFile="images/bitshares.icns"
deployArgs="-qmldir=../qml"
finalDMGName="BitShares Light Wallet.dmg"
size=200m

mkdir $source
cp -r "${title}.app" "${source}/${title}.app"

cd "${source}/"
$macdeployqt "${title}.app" $deployArgs
mv "${title}.app" "${applicationName}.app"
cd -
cp "${iconFile}" "${source}/${applicationName}.app/Contents/Resources/"
rm -f "${finalDMGName}"

hdiutil create -srcfolder "${source}" -volname "${applicationName}" -fs HFS+ \
         -fsargs "-c c=64,a=16,e=16" -format UDRW -size ${size} pack.temp.dmg
device=$(hdiutil attach -readwrite -noverify -noautoopen "pack.temp.dmg" | \
         egrep '^/dev/' | sed 1q | awk '{print $1}')
echo $device

sleep 3

echo '
   tell application "Finder"
     tell disk "'${applicationName}'"
           open
           set current view of container window to icon view
           set toolbar visible of container window to false
           set statusbar visible of container window to false
           set the bounds of container window to {400, 100, 942, 392}
           set theViewOptions to the icon view options of container window
           set arrangement of theViewOptions to not arranged
           set icon size of theViewOptions to 192
           #set background picture of theViewOptions to file ".background:'${backgroundPictureName}'"
           make new alias file at container window to POSIX file "/Applications" with properties {name:"Applications"}
           set position of item "'${applicationName}'" of container window to {100, 100}
           set position of item "Applications" of container window to {350, 100}
           update without registering applications
           delay 5
           close
     end tell
   end tell
' | osascript

sleep 3

chmod -Rf go-w /Volumes/"${applicationName}"
sync
sync
hdiutil detach ${device}
hdiutil convert "pack.temp.dmg" -format UDZO -imagekey zlib-level=9 -o "${finalDMGName}"
rm -f pack.temp.dmg
rm -rf "${source}"
