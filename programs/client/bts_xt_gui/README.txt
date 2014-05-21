== HTML5 GUI + QT WebKit Conatainer ==

This project consists of two parts:
1. QT application in qtapp directory
2. HTML5 web application, located in webapp directory


-- Configuration and installation --


1. QT application:

Download and install QT5, see http://qt-project.org/downloads
Set envronment variable CMAKE_PREFIX_PATH to point to clang_64 in your QT directory, e.g.:
> export CMAKE_PREFIX_PATH=/Users/user/Qt5.2.1/5.2.1/clang_64

Use CMake to configure bitshares_toolkit, set INCLUDE_GUI to TRUE or to ON, e.g.
> cmake -DINCLUDE_GUI=ON ../bitshares_toolkit

If there were no compilation errors, the executable should be located in programs/bts_xt/bts_xt_gui/qtapp/bts_xt_gui.
Now you need to run it in a way similar to bts_xt_client - it accepts the same command line parameters as bts_xt_client or reads them from config.json.

Here is an example:
> bts_xt_gui --data-dir /tmp/bts_xt_gui --trustee-address 9gWvCSLaAA67Rwg9AEvPAttUrdr9pXXKk --connect-to 107.170.30.182:8765

Note. Before starting bts_xt_gui please edit config.json and specify htdocs path as either programs/bts_xt/bts_xt_gui/webapp/dist/ (production) or programs/bts_xt/bts_xt_gui/webapp/generated/ (dev) or original one from programs/bts_xt.


2. Web application:

The app uses Lineman.js to compile assets from app and vendor dirs on the fly, the output goes into dist directory.
The installation is very simple, basically you need to install node.js
and install Lineman and dependencies via the following commands:
> npm install -g lineman  
> npm install
(find more Lineman.js documentation here http://www.linemanjs.com/)

To test Web application you need:
1. Specify programs/bts_xt/bts_xt_gui/webapp/generated as htdocs in client's config.json.
2. Start either bts_xt_client (with --server option) or bts_xt_gui (no --server needed).
3. Go to programs/bts_xt/bts_xt_gui/webapp/ and start lineman: 
> lineman run
4. Open http://localhost:9989 and make sure that the application is working.
5. Now you can edit application's html, js and css files located in programs/bts_xt/bts_xt_gui/webapp/app.
