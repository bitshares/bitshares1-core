#!/usr/bin/env ruby

# Copies missing Info.plist files for a .app's Qt frameworks installed
# from macdeployqt. Without the plists, 'codesign' fails on 10.9 with
# "bundle format unrecognized, invalid, or unsuitable".
#
# Example usage:
# ruby macdeployqt_fix_frameworks.rb /Users/me/Qt5.2.0/5.2.0/clang_64/ MyProgram.app
#
# Links:
# https://bugreports.qt-project.org/browse/QTBUG-23268
# http://stackoverflow.com/questions/19637131/sign-a-framework-for-osx-10-9
# http://qt-project.org/forums/viewthread/35312

require 'fileutils'

qtdir = ARGV.shift
dotapp = ARGV.shift

abort "Missing args." if !qtdir || !dotapp
abort "\"#{qtdir}\" invalid" if !File.exists?(qtdir) || !File.directory?(qtdir)
abort "\"#{dotapp}\" invalid" if !File.exists?(dotapp) || !File.directory?(dotapp)

frameworksDir = File.join(dotapp, 'Contents', 'Frameworks')
Dir.foreach(frameworksDir) do |framework|
  next if !framework.match(/^Qt.*.framework$/)
  fullPath = File.join(frameworksDir, framework)
  destPlist = File.join(fullPath, 'Resources', 'Info.plist')
  next if File.exists?(destPlist)
  srcPlist = File.join(qtdir, 'lib', framework, 'Contents', 'Info.plist')
  abort "Source plist not found: \"#{srcPlist}\"" if !File.exists?(srcPlist)
  FileUtils.cp(srcPlist, destPlist)
end
