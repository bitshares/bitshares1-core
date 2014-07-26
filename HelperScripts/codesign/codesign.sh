# This codesign of apple is stupid, seem have problem with recursive codesign and --deep flag
# and with CMake/CPack's make package, haven't find way to code sign before package, because the dependency Frameworks and plugins seems install in run time, which will change the signature
# So we have to extract the BitsharesX.app from the dmg, and codesign it manually


XCODE_PATH_CONTENT_DEVELOPER=`xcode-select -print-path`
if [ -f "${XCODE_PATH_CONTENT_DEVELOPER}/Toolchains/XcodeDefault.xctoolchain/usr/bin/codesign_allocate" ]; then
  export CODESIGN_ALLOCATE="${XCODE_PATH_CONTENT_DEVELOPER}/Toolchains/XcodeDefault.xctoolchain/usr/bin/codesign_allocate"
elif [ -f "${XCODE_PATH_CONTENT_DEVELOPER}/usr/bin/codesign_allocate" ]; then
  export CODESIGN_ALLOCATE="${XCODE_PATH_CONTENT_DEVELOPER}/usr/bin/codesign_allocate"
else
  export CODESIGN_ALLOCATE="/usr/bin/codesign_allocate"
fi

find ./ -type d -print | sed -e 's;[^/]*/;|____;g;s;____|; |;g'

echo $CODESIGN_ALLOCATE

mkdir -p BitSharesX.app/Contents/Frameworks/QtCore.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtCore.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtCore.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtCore.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtGui.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtGui.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtGui.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtGui.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtMultimedia.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtMultimedia.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtMultimedia.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtMultimedia.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtMultimediaWidgets.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtMultimediaWidgets.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtMultimediaWidgets.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtMultimediaWidgets.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtNetwork.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtNetwork.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtNetwork.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtNetwork.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtOpenGL.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtOpenGL.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtOpenGL.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtOpenGL.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtPositioning.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtPositioning.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtPositioning.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtPositioning.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtPrintSupport.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtPrintSupport.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtPrintSupport.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtPrintSupport.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtQml.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtQml.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtQml.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtQml.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtQuick.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtQuick.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtQuick.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtQuick.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtSensors.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtSensors.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtSensors.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtSensors.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtSql.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtSql.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtSql.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtSql.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtWebKit.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtWebkit.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtWebKit.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtWebKit.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtWebKitWidgets.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtWebKitWidgets.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtWebKitWidgets.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtWebKitWidgets.framework

mkdir -p BitSharesX.app/Contents/Frameworks/QtWidgets.framework/Resources/
cp ~/Qt/5.3/clang_64/lib/QtWidgets.framework/Contents/Info.plist BitSharesX.app/Contents/Frameworks/QtWidgets.framework/Resources/
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/Frameworks/QtWidgets.framework

codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/PlugIns/imageformats/libqjpeg.dylib
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/PlugIns/platforms/libqcocoa.dylib
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/MacOS/libdb_cxx-5.3.dylib
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/MacOS/libgcc_s.1.dylib
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/MacOS/libreadline.6.dylib
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app/Contents/MacOS/libstdc++.6.dylib
codesign --force --verify --verbose --sign "Developer ID Application: SHA BIT" BitSharesX.app

mkdir -p BitSharesX
mv BitSharesX.app ./BitSharesX/BitSharesX.app
ln -s /Applications/ ./BitSharesX/Applications
sudo spctl -a -v BitSharesX/BitSharesX.app
hdiutil create -size 130m ./BitSharesX.dmg -srcfolder ./BitSharesX/ -ov
